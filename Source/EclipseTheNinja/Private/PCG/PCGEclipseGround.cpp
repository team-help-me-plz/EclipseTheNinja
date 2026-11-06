// Fill out your copyright notice in the Description page of Project Settings.

#include "PCG/PCGEclipseGround.h"

#include "PCG/EclipseGroundVolume.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Volume.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Math/RandomStream.h"

namespace EclipseGround
{
    const FName MeshAttributeName = TEXT("Mesh");
    const FName ExclusionTag = TEXT("PCG_Exclusion");

    // 흩뿌림 무리 반경(cm)
    const double ClumpRadius = 450.0;

    // 출력할 메시 하나
    struct FPiece
    {
        FSoftObjectPath Mesh;
        FTransform Transform;
    };

    // 가중치 후보 중 하나를 고른다. 후보가 없으면 null
    template <typename EntryType>
    const EntryType* PickWeighted(const TArray<EntryType>& Entries, FRandomStream& Stream)
    {
        float Total = 0.0f;
        for (const EntryType& Entry : Entries)
        {
            if (!Entry.Mesh.IsNull())
            {
                Total += FMath::Max(Entry.Weight, 0.0f);
            }
        }

        if (Total <= 0.0f)
        {
            return nullptr;
        }

        float Roll = static_cast<float>(Stream.FRandRange(0.0, Total));
        const EntryType* LastValid = nullptr;
        for (const EntryType& Entry : Entries)
        {
            if (Entry.Mesh.IsNull())
            {
                continue;
            }

            Roll -= FMath::Max(Entry.Weight, 0.0f);
            if (Roll <= 0.0f)
            {
                return &Entry;
            }

            LastValid = &Entry;
        }

        // 부동소수 오차로 끝까지 남으면 마지막 유효 후보
        return LastValid;
    }

    // 타일 한 장 크기: 사용자 값, 없으면 첫 타일 메시의 가로 폭
    double ResolveTileSize(const FEclipseGroundParams& Params)
    {
        // 너무 작은 값이면 타일 수가 폭주하므로 100cm 아래는 받지 않는다
        if (Params.TileSize > 0.0f)
        {
            return FMath::Max<double>(Params.TileSize, 100.0);
        }

        for (const FEclipseGroundTileEntry& Entry : Params.Tiles)
        {
            const UStaticMesh* Mesh = Entry.Mesh.LoadSynchronous();
            if (Mesh)
            {
                return FMath::Max(Mesh->GetBounds().BoxExtent.X * 2.0, 1.0);
            }
        }

        return 0.0;
    }

    // 볼륨 로컬 사각형을 타일로 채운다
    void AddTiles(
        const FEclipseGroundParams& Params,
        const FVector& HalfSize,
        double SurfaceZ,
        FRandomStream& Stream,
        TArray<FPiece>& OutPieces
    )
    {
        const double TileSize = ResolveTileSize(Params);
        if (TileSize <= 0.0)
        {
            return;
        }

        // 맞춰 늘이기면 가까운 정수 장수로 나누고, 아니면 덮을 만큼 올림
        const FVector2D Area(HalfSize.X * 2.0, HalfSize.Y * 2.0);
        const int32 CountX = Params.bFitTilesToVolume
            ? FMath::Max(1, FMath::RoundToInt32(Area.X / TileSize))
            : FMath::Max(1, FMath::CeilToInt32(Area.X / TileSize));
        const int32 CountY = Params.bFitTilesToVolume
            ? FMath::Max(1, FMath::RoundToInt32(Area.Y / TileSize))
            : FMath::Max(1, FMath::CeilToInt32(Area.Y / TileSize));

        const double StepX = Params.bFitTilesToVolume ? Area.X / CountX : TileSize;
        const double StepY = Params.bFitTilesToVolume ? Area.Y / CountY : TileSize;
        const FVector BaseScale(StepX / TileSize, StepY / TileSize, 1.0);

        for (int32 IndexY = 0; IndexY < CountY; ++IndexY)
        {
            for (int32 IndexX = 0; IndexX < CountX; ++IndexX)
            {
                const FEclipseGroundTileEntry* Entry = PickWeighted(Params.Tiles, Stream);
                if (!Entry)
                {
                    continue;
                }

                // 90도 단위 회전. 90·270도면 늘이는 축이 바뀌므로 스케일 X·Y를 바꾼다
                const int32 Quarter = Params.bRandomTileRotation ? Stream.RandRange(0, 3) : 0;
                const FVector Scale = (Quarter % 2 == 1)
                    ? FVector(BaseScale.Y, BaseScale.X, 1.0)
                    : BaseScale;

                const FVector Location(
                    -HalfSize.X + StepX * (IndexX + 0.5),
                    -HalfSize.Y + StepY * (IndexY + 0.5),
                    SurfaceZ
                );

                FPiece& Piece = OutPieces.AddDefaulted_GetRef();
                Piece.Mesh = Entry->Mesh.ToSoftObjectPath();
                Piece.Transform = FTransform(FRotator(0.0, Quarter * 90.0, 0.0), Location, Scale);
            }
        }
    }

    // 최소 간격 검사용 격자 키
    FIntPoint CellOf(const FVector2D& Point, double CellSize)
    {
        return FIntPoint(
            FMath::FloorToInt32(Point.X / CellSize),
            FMath::FloorToInt32(Point.Y / CellSize)
        );
    }

    // 덤불·돌을 흩뿌린다. 위치는 볼륨 로컬, 제외 판정은 월드
    void AddScatter(
        const FEclipseGroundParams& Params,
        const FVector& HalfSize,
        double SurfaceZ,
        const FTransform& VolumeTransform,
        const TArray<FBox>& ExcludeBoxes,
        FRandomStream& Stream,
        TArray<FPiece>& OutPieces
    )
    {
        const double AreaM2 = (HalfSize.X * 2.0 / 100.0) * (HalfSize.Y * 2.0 / 100.0);
        const int32 TargetCount = FMath::FloorToInt32(AreaM2 / 100.0 * Params.ScatterDensity);
        if (TargetCount <= 0 || Params.Scatter.IsEmpty())
        {
            return;
        }

        // 뭉침: 무리 중심을 미리 뽑고 일부 점을 그 주변에 모은다
        TArray<FVector2D> ClumpCenters;
        const int32 ClumpCount = FMath::Max(1, TargetCount / 8);
        for (int32 Index = 0; Index < ClumpCount; ++Index)
        {
            ClumpCenters.Add(FVector2D(
                Stream.FRandRange(-HalfSize.X, HalfSize.X),
                Stream.FRandRange(-HalfSize.Y, HalfSize.Y)
            ));
        }

        const double Spacing = FMath::Max<double>(Params.ScatterMinSpacing, 1.0);
        TMap<FIntPoint, TArray<FVector2D>> Placed;
        int32 PlacedCount = 0;

        // 간격·제외에 걸려 버려지는 점을 감안해 시도 횟수를 넉넉히
        for (int32 Attempt = 0; Attempt < TargetCount * 4 && PlacedCount < TargetCount; ++Attempt)
        {
            FVector2D Point;
            if (Stream.FRand() < Params.ScatterClumping)
            {
                const FVector2D& Center = ClumpCenters[Stream.RandRange(0, ClumpCount - 1)];
                const double Radius = ClumpRadius * FMath::Sqrt(Stream.FRand());
                const double Angle = Stream.FRandRange(0.0, UE_TWO_PI);
                Point = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
            }
            else
            {
                Point = FVector2D(
                    Stream.FRandRange(-HalfSize.X, HalfSize.X),
                    Stream.FRandRange(-HalfSize.Y, HalfSize.Y)
                );
            }

            const bool bInside = FMath::Abs(Point.X) <= HalfSize.X && FMath::Abs(Point.Y) <= HalfSize.Y;
            if (!bInside)
            {
                continue;
            }

            // 플레이 면 표지 볼륨 안이나 그 앞(카메라 쪽 +Y)이면 버린다. 앞쪽 덤불은 화면을 가린다
            const FVector WorldPoint = VolumeTransform.TransformPosition(FVector(Point, SurfaceZ));
            const bool bExcluded = ExcludeBoxes.ContainsByPredicate([&WorldPoint](const FBox& Box)
            {
                const bool bInsideX = WorldPoint.X >= Box.Min.X && WorldPoint.X <= Box.Max.X;
                return bInsideX && WorldPoint.Y >= Box.Min.Y;
            });

            if (bExcluded)
            {
                continue;
            }

            // 주변 격자 칸만 보고 최소 간격 검사
            const FIntPoint Cell = CellOf(Point, Spacing);
            bool bTooClose = false;
            for (int32 OffsetY = -1; OffsetY <= 1 && !bTooClose; ++OffsetY)
            {
                for (int32 OffsetX = -1; OffsetX <= 1 && !bTooClose; ++OffsetX)
                {
                    const TArray<FVector2D>* Neighbors = Placed.Find(Cell + FIntPoint(OffsetX, OffsetY));
                    if (!Neighbors)
                    {
                        continue;
                    }

                    for (const FVector2D& Other : *Neighbors)
                    {
                        if (FVector2D::DistSquared(Other, Point) < Spacing * Spacing)
                        {
                            bTooClose = true;
                            break;
                        }
                    }
                }
            }

            if (bTooClose)
            {
                continue;
            }

            const FEclipseGroundScatterEntry* Entry = PickWeighted(Params.Scatter, Stream);
            if (!Entry)
            {
                return;
            }

            const double Scale = Stream.FRandRange(
                FMath::Min(Entry->ScaleMin, Entry->ScaleMax),
                FMath::Max(Entry->ScaleMin, Entry->ScaleMax)
            );

            FPiece& Piece = OutPieces.AddDefaulted_GetRef();
            Piece.Mesh = Entry->Mesh.ToSoftObjectPath();
            Piece.Transform = FTransform(
                FRotator(0.0, Stream.FRandRange(0.0, 360.0), 0.0),
                FVector(Point, SurfaceZ),
                FVector(Scale)
            );

            Placed.FindOrAdd(Cell).Add(Point);
            ++PlacedCount;
        }
    }

    // 조각 목록을 Mesh 속성이 있는 포인트 데이터로 내보낸다
    void WritePieces(FPCGContext* Context, const TArray<FPiece>& Pieces)
    {
        UPCGBasePointData* OutPoints = FPCGContext::NewPointData_AnyThread(Context);
        OutPoints->SetNumPoints(Pieces.Num(), /*bInitializeValues=*/false);
        OutPoints->AllocateProperties(EPCGPointNativeProperties::All);

        UPCGMetadata* Metadata = OutPoints->MutableMetadata();
        FPCGMetadataAttribute<FSoftObjectPath>* MeshAttribute = Metadata->FindOrCreateAttribute<FSoftObjectPath>(
            MeshAttributeName,
            FSoftObjectPath(),
            /*bAllowsInterpolation=*/false,
            /*bOverrideParent=*/true
        );

        FPCGPointValueRanges OutRanges(OutPoints, /*bAllocate=*/false);
        for (int32 Index = 0; Index < Pieces.Num(); ++Index)
        {
            FPCGPoint Point;
            Point.Transform = Pieces[Index].Transform;
            Point.Density = 1.0f;
            Point.Steepness = 1.0f;
            Point.BoundsMin = FVector(-50.0);
            Point.BoundsMax = FVector(50.0);
            Point.Seed = PCGHelpers::ComputeSeedFromPosition(Point.Transform.GetLocation());
            Point.MetadataEntry = Metadata->AddEntry();

            MeshAttribute->SetValue(Point.MetadataEntry, Pieces[Index].Mesh);
            OutRanges.SetFromPoint(Index, Point);
        }

        FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
        Output.Data = OutPoints;
        Output.Pin = PCGPinConstants::DefaultOutputLabel;
    }
}

TArray<FPCGPinProperties> UPCGEclipseGroundSettings::InputPinProperties() const
{
    TArray<FPCGPinProperties> PinProperties;
    PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial);
    return PinProperties;
}

TArray<FPCGPinProperties> UPCGEclipseGroundSettings::OutputPinProperties() const
{
    TArray<FPCGPinProperties> PinProperties;
    PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
    return PinProperties;
}

FPCGElementPtr UPCGEclipseGroundSettings::CreateElement() const
{
    return MakeShared<FPCGEclipseGroundElement>();
}

bool FPCGEclipseGroundElement::ExecuteInternal(FPCGContext* Context) const
{
    using namespace EclipseGround;

    const UPCGComponent* Component = Cast<UPCGComponent>(Context->ExecutionSource.GetObject());
    const AEclipseGroundVolume* Volume = Component
        ? Cast<AEclipseGroundVolume>(Component->GetOwner())
        : nullptr;

    if (!Volume)
    {
        PCGE_LOG(Warning, GraphAndLog, NSLOCTEXT("PCGEclipseGround", "NoVolume", "Eclipse Ground needs an Eclipse Ground Volume as the execution source."));
        return true;
    }

    const FEclipseGroundParams& Params = Volume->GetGroundParams();
    const FVector HalfSize = Volume->GetBoxSize() * 0.5;
    const double SurfaceZ = HalfSize.Z + Params.SurfaceOffset;
    FRandomStream Stream(Params.Seed);

    // 플레이 면 표지 볼륨 바운드
    TArray<FBox> ExcludeBoxes;
    UWorld* World = Volume->GetWorld();
    if (Params.bAvoidPlayArea && World)
    {
        for (TActorIterator<AVolume> It(World); It; ++It)
        {
            if (It->ActorHasTag(ExclusionTag))
            {
                ExcludeBoxes.Add(It->GetBounds().GetBox());
            }
        }
    }

    // 위치는 볼륨 로컬로 만들고 마지막에 월드로 옮긴다
    TArray<FPiece> Pieces;
    AddTiles(Params, HalfSize, SurfaceZ, Stream, Pieces);
    AddScatter(
        Params,
        HalfSize,
        SurfaceZ,
        Volume->GetActorTransform(),
        ExcludeBoxes,
        Stream,
        Pieces
    );

    const FTransform VolumeTransform = Volume->GetActorTransform();
    for (FPiece& Piece : Pieces)
    {
        Piece.Transform = Piece.Transform * VolumeTransform;
    }

    WritePieces(Context, Pieces);
    return true;
}
