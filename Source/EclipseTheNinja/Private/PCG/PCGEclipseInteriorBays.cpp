// Fill out your copyright notice in the Description page of Project Settings.

#include "PCG/PCGEclipseInteriorBays.h"

#include "PCG/EclipseInteriorVolume.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Math/RandomStream.h"

namespace EclipseInteriorBays
{
    const FName MeshAttributeName = TEXT("Mesh");
    const FName ShadowBlockersLabel = TEXT("ShadowBlockers");

    // 출력할 메시 하나
    struct FPiece
    {
        FSoftObjectPath Mesh;
        FTransform Transform;
    };

    // 가중치 후보 중 하나를 고른다. 후보가 없거나 빈 후보가 뽑히면 빈 경로
    template <typename EntryType>
    const EntryType* PickWeighted(const TArray<EntryType>& Entries, FRandomStream& Stream)
    {
        float Total = 0.0f;
        for (const EntryType& Entry : Entries)
        {
            Total += FMath::Max(Entry.Weight, 0.0f);
        }

        if (Total <= 0.0f)
        {
            return nullptr;
        }

        float Roll = static_cast<float>(Stream.FRandRange(0.0, Total));
        for (const EntryType& Entry : Entries)
        {
            Roll -= FMath::Max(Entry.Weight, 0.0f);
            if (Roll <= 0.0f)
            {
                return &Entry;
            }
        }

        return &Entries.Last();
    }

    // 메시가 있으면 조각 목록에 넣는다
    void AddPiece(
        TArray<FPiece>& Pieces,
        const TSoftObjectPtr<UStaticMesh>& Mesh,
        const FVector& Location,
        const FVector& Scale,
        float Yaw = 0.0f
    )
    {
        if (Mesh.IsNull())
        {
            return;
        }

        FPiece& Piece = Pieces.AddDefaulted_GetRef();
        Piece.Mesh = Mesh.ToSoftObjectPath();
        Piece.Transform = FTransform(FRotator(0.0f, Yaw, 0.0f), Location, Scale);
    }

    // 칸이 변형 칸인지 판정한다
    bool IsVariantBay(const FEclipseInteriorRoomParams& Params, int32 BayIndex, int32 NumBays, FRandomStream& Stream)
    {
        const bool bEndBay = BayIndex == 0 || BayIndex == NumBays - 1;
        if (Params.bKeepEndBaysPlain && NumBays > 2 && bEndBay)
        {
            return false;
        }

        if (Params.VariantMode == EEclipseInteriorVariantMode::Ratio)
        {
            return Stream.FRand() < Params.VariantRatio;
        }

        if (Params.VariantEvery <= 0)
        {
            return false;
        }

        // 오프셋 0, Every 3이면 2·5·8번 칸
        const int32 Shifted = BayIndex + Params.BayOffset + 1;
        return ((Shifted % Params.VariantEvery) + Params.VariantEvery) % Params.VariantEvery == 0;
    }

    // 실행 소스(PCG 컴포넌트)의 주인 액터
    AActor* GetSourceActor(FPCGContext* Context)
    {
        UPCGComponent* Component = Cast<UPCGComponent>(Context->ExecutionSource.GetObject());
        return Component ? Component->GetOwner() : nullptr;
    }

    // 조각 목록을 Mesh 속성이 있는 포인트 데이터로 만들어 지정한 핀으로 내보낸다
    void WritePieces(FPCGContext* Context, const TArray<FPiece>& Pieces, FName PinLabel)
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
        Output.Pin = PinLabel;
    }
}

TArray<FPCGPinProperties> UPCGEclipseInteriorBaysSettings::InputPinProperties() const
{
    TArray<FPCGPinProperties> PinProperties;
    PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial);
    return PinProperties;
}

TArray<FPCGPinProperties> UPCGEclipseInteriorBaysSettings::OutputPinProperties() const
{
    TArray<FPCGPinProperties> PinProperties;
    PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
    PinProperties.Emplace(EclipseInteriorBays::ShadowBlockersLabel, EPCGDataType::Point);
    return PinProperties;
}

FPCGElementPtr UPCGEclipseInteriorBaysSettings::CreateElement() const
{
    return MakeShared<FPCGEclipseInteriorBaysElement>();
}

bool FPCGEclipseInteriorBaysElement::ExecuteInternal(FPCGContext* Context) const
{
    using namespace EclipseInteriorBays;

    const UPCGEclipseInteriorBaysSettings* Settings = Context->GetInputSettings<UPCGEclipseInteriorBaysSettings>();
    check(Settings);

    // 방 파라미터: 내부 볼륨이면 액터 값, 아니면 노드의 Fallback
    AActor* SourceActor = GetSourceActor(Context);
    const AEclipseInteriorVolume* InteriorVolume = Cast<AEclipseInteriorVolume>(SourceActor);
    const FEclipseInteriorRoomParams& Params = InteriorVolume
        ? InteriorVolume->GetRoomParams()
        : Settings->FallbackParams;

    // 방 바운드: 입력 공간 데이터, 없으면 주인 액터 바운드
    FBox Bounds(EForceInit::ForceInit);
    for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
    {
        if (const UPCGSpatialData* Spatial = Cast<UPCGSpatialData>(Input.Data))
        {
            Bounds += Spatial->GetBounds();
        }
    }

    if (!Bounds.IsValid && SourceActor)
    {
        Bounds = SourceActor->GetComponentsBoundingBox(/*bNonColliding=*/true);
    }

    if (!Bounds.IsValid)
    {
        PCGE_LOG(Warning, GraphAndLog, NSLOCTEXT("PCGEclipseInteriorBays", "NoBounds", "No room bounds (connect the volume input or use a PCG volume)."));
        return true;
    }

    const double X0 = Bounds.Min.X;
    const double X1 = Bounds.Max.X;
    const double BackY = Bounds.Min.Y;
    const double FrontY = Bounds.Max.Y;
    const double FloorZ = Bounds.Min.Z;
    const double CeilZ = Bounds.Max.Z;
    const double Height = CeilZ - FloorZ;
    const double Depth = FrontY - BackY;

    // 칸 수와 실제 칸 폭(길이에 맞게 늘이거나 줄임)
    const int32 NumBays = FMath::Max(1, FMath::RoundToInt((X1 - X0) / Params.BayWidth));
    const double BayW = (X1 - X0) / NumBays;
    const double BayScaleX = BayW / Settings->PrefabWidth;

    // 중간 층 위치(바닥 앞 가장자리보다 앞으로 나가지 않게)
    const bool bMidLayer = Params.bSpawnMidLayer && Params.MidLayerDistance <= Depth - 50.0 + KINDA_SMALL_NUMBER;
    const double MidY = BackY + Params.MidLayerDistance;

    // 소품 띠: 뒷벽 앞 ~ 중간 층 뒤. 얕은 방에서도 소품이 앞 가장자리(플레이 블록) 밖으로 나오지 않게 120 안쪽까지
    const double PropYMin = BackY + 60.0;
    const double PropYMax = FMath::Min(
        (bMidLayer ? MidY : BackY + Params.MidLayerDistance) - 60.0,
        FrontY - 120.0
    );

    TArray<FPiece> Pieces;

    for (int32 Bay = 0; Bay < NumBays; ++Bay)
    {
        FRandomStream Stream(PCGHelpers::ComputeSeed(Params.Seed, Bay, 7919));
        const double CenterX = X0 + BayW * (Bay + 0.5);

        // 뒷벽: 기본/변형 칸 선택
        const bool bVariant = IsVariantBay(Params, Bay, NumBays, Stream) && Settings->VariantWallMeshes.Num() > 0;
        const TArray<FEclipseInteriorMeshEntry>& WallCandidates = bVariant
            ? Settings->VariantWallMeshes
            : Settings->WallMeshes;

        const double BaseScaleZ = Height < Settings->WallBaseHeight
            ? Height / Settings->WallBaseHeight
            : 1.0;

        if (const FEclipseInteriorMeshEntry* Wall = PickWeighted(WallCandidates, Stream))
        {
            AddPiece(
                Pieces,
                Wall->Mesh,
                FVector(CenterX, BackY, FloorZ),
                FVector(BayScaleX, 1.0, BaseScaleZ)
            );
        }

        // 벽 위 띠벽을 천장까지 쌓는다(마지막 몇 장은 높이를 나눠 맞춤)
        const double Remain = Height - Settings->WallBaseHeight;
        if (Remain > 1.0)
        {
            const int32 NumBands = FMath::Max(1, FMath::CeilToInt(Remain / Settings->WallTopHeight - 0.01));
            const double BandH = Remain / NumBands;
            for (int32 Band = 0; Band < NumBands; ++Band)
            {
                AddPiece(
                    Pieces,
                    Settings->WallTopMesh,
                    FVector(CenterX, BackY, FloorZ + Settings->WallBaseHeight + BandH * Band),
                    FVector(BayScaleX, 1.0, BandH / Settings->WallTopHeight)
                );
            }
        }

        // 중간 층 칸
        if (bMidLayer)
        {
            if (const FEclipseInteriorMeshEntry* Mid = PickWeighted(Settings->MidLayerBayMeshes, Stream))
            {
                AddPiece(
                    Pieces,
                    Mid->Mesh,
                    FVector(CenterX, MidY, FloorZ),
                    FVector(BayScaleX, 1.0, 1.0)
                );
            }
        }

        // 바닥·천장 타일(깊이 방향 줄 수도 길이에 맞게 나눔)
        if (Params.bSpawnFloor)
        {
            const int32 Rows = FMath::Max(1, FMath::RoundToInt(Depth / Settings->FloorTileSize));
            const double RowD = Depth / Rows;
            for (int32 Row = 0; Row < Rows; ++Row)
            {
                AddPiece(
                    Pieces,
                    Settings->FloorMesh,
                    FVector(CenterX, BackY + RowD * (Row + 0.5), FloorZ + Settings->FloorZOffset),
                    FVector(BayW / Settings->FloorTileSize, RowD / Settings->FloorTileSize, 1.0)
                );
            }
        }

        // 바닥 앞 가장자리 아래 판벽(구덩이 뒤를 막음)
        if (Params.FrontSkirtDepth > 1.0f)
        {
            AddPiece(
                Pieces,
                Settings->FrontSkirtMesh,
                FVector(CenterX, FrontY, FloorZ - Params.FrontSkirtDepth),
                FVector(BayScaleX, 1.0, Params.FrontSkirtDepth / Settings->FrontSkirtHeight)
            );
        }

        if (Params.bSpawnCeiling)
        {
            const int32 Rows = FMath::Max(1, FMath::RoundToInt(Depth / Settings->CeilingTileSize));
            const double RowD = Depth / Rows;
            for (int32 Row = 0; Row < Rows; ++Row)
            {
                AddPiece(
                    Pieces,
                    Settings->CeilingMesh,
                    FVector(CenterX, BackY + RowD * (Row + 0.5), CeilZ),
                    FVector(BayW / Settings->CeilingTileSize, RowD / Settings->CeilingTileSize, 1.0)
                );
            }
        }

        // 소품: 칸을 자리 수로 나눠 자리마다 확률로 하나
        const int32 Slots = Settings->PropSlotsPerBay;
        if (Slots > 0 && Params.PropDensity > 0.0f && Settings->Props.Num() > 0 && PropYMax > PropYMin)
        {
            const double SlotW = BayW / Slots;
            for (int32 Slot = 0; Slot < Slots; ++Slot)
            {
                if (Stream.FRand() >= Params.PropDensity)
                {
                    continue;
                }

                const FEclipseInteriorPropEntry* Prop = PickWeighted(Settings->Props, Stream);
                if (!Prop)
                {
                    continue;
                }

                const double SlotX0 = X0 + BayW * Bay + SlotW * Slot;
                const double Inset = FMath::Min(50.0, SlotW * 0.25);
                const FVector Location(
                    Stream.FRandRange(SlotX0 + Inset, SlotX0 + SlotW - Inset),
                    Stream.FRandRange(PropYMin, PropYMax),
                    FloorZ
                );
                const float Scale = Prop->Scale * static_cast<float>(Stream.FRandRange(0.9, 1.1));
                const float Yaw = static_cast<float>(Stream.FRandRange(-Settings->PropYawJitter, Settings->PropYawJitter));

                AddPiece(
                    Pieces,
                    Prop->Mesh,
                    Location,
                    FVector(Scale),
                    Yaw
                );
            }
        }
    }

    // 칸 경계 기둥(뒷벽, 중간 층)
    const double ColumnScaleZ = Height / Settings->ColumnHeight;
    for (int32 Edge = 0; Edge <= NumBays; ++Edge)
    {
        const double EdgeX = X0 + BayW * Edge;
        AddPiece(
            Pieces,
            Settings->ColumnMesh,
            FVector(EdgeX, BackY + Settings->ColumnWallOffset, FloorZ),
            FVector(1.0, 1.0, ColumnScaleZ)
        );

        if (bMidLayer && Settings->bMidLayerColumns)
        {
            AddPiece(
                Pieces,
                Settings->ColumnMesh,
                FVector(EdgeX, MidY, FloorZ),
                FVector(1.0, 1.0, ColumnScaleZ)
            );
        }
    }

    // 양 끝 옆벽: 기본 칸 벽 첫 후보를 90도 돌려 깊이 방향으로 세운다(왼쪽은 +X, 오른쪽은 -X를 본다)
    const double CenterY = (BackY + FrontY) * 0.5;
    const double DepthScaleX = Depth / Settings->PrefabWidth;
    const double EndBaseScaleZ = Height < Settings->WallBaseHeight
        ? Height / Settings->WallBaseHeight
        : 1.0;
    const double EndRemain = Height - Settings->WallBaseHeight;
    const int32 EndBands = EndRemain > 1.0
        ? FMath::Max(1, FMath::CeilToInt(EndRemain / Settings->WallTopHeight - 0.01))
        : 0;

    const TSoftObjectPtr<UStaticMesh> EndWallMesh = Settings->WallMeshes.Num() > 0
        ? Settings->WallMeshes[0].Mesh
        : TSoftObjectPtr<UStaticMesh>();

    TArray<TPair<double, float>> EndWalls;
    if (Params.bEndWallLeft)
    {
        EndWalls.Emplace(X0, -90.0f);
    }

    if (Params.bEndWallRight)
    {
        EndWalls.Emplace(X1, 90.0f);
    }

    for (const TPair<double, float>& EndWall : EndWalls)
    {
        AddPiece(
            Pieces,
            EndWallMesh,
            FVector(EndWall.Key, CenterY, FloorZ),
            FVector(DepthScaleX, 1.0, EndBaseScaleZ),
            EndWall.Value
        );

        for (int32 Band = 0; Band < EndBands; ++Band)
        {
            const double BandH = EndRemain / EndBands;
            AddPiece(
                Pieces,
                Settings->WallTopMesh,
                FVector(EndWall.Key, CenterY, FloorZ + Settings->WallBaseHeight + BandH * Band),
                FVector(DepthScaleX, 1.0, BandH / Settings->WallTopHeight),
                EndWall.Value
            );
        }
    }

    // 그림자 차단 상자: 뒷벽 뒤, 천장 위, 옆벽 바깥(100cm 큐브 기준 스케일)
    TArray<FPiece> Blockers;
    if (Params.bShadowBlocker)
    {
        const double Thick = 0.2;
        const double BlockBackY = BackY - Settings->ShadowBlockerBackOffset;
        const double BlockDepth = FrontY - BlockBackY;
        const double MidZ = (FloorZ + CeilZ) * 0.5;

        // 옆벽이 있는 쪽은 옆 차단판(바깥 40)까지 덮도록 뒷면·윗면 판을 60 늘려 모서리 틈을 없앤다
        const double BlockX0 = X0 - (Params.bEndWallLeft ? 60.0 : 0.0);
        const double BlockX1 = X1 + (Params.bEndWallRight ? 60.0 : 0.0);

        AddPiece(
            Blockers,
            Settings->ShadowBlockerMesh,
            FVector((BlockX0 + BlockX1) * 0.5, BlockBackY, MidZ),
            FVector((BlockX1 - BlockX0) / 100.0, Thick, (Height + 100.0) / 100.0)
        );

        AddPiece(
            Blockers,
            Settings->ShadowBlockerMesh,
            FVector((BlockX0 + BlockX1) * 0.5, (BlockBackY + FrontY) * 0.5, CeilZ + 40.0),
            FVector((BlockX1 - BlockX0) / 100.0, BlockDepth / 100.0, Thick)
        );

        for (const TPair<double, float>& EndWall : EndWalls)
        {
            const double Outward = EndWall.Value < 0.0f ? -40.0 : 40.0;
            AddPiece(
                Blockers,
                Settings->ShadowBlockerMesh,
                FVector(EndWall.Key + Outward, (BlockBackY + FrontY) * 0.5, MidZ),
                FVector(Thick, BlockDepth / 100.0, (Height + 100.0) / 100.0)
            );
        }
    }

    WritePieces(Context, Pieces, PCGPinConstants::DefaultOutputLabel);
    WritePieces(Context, Blockers, ShadowBlockersLabel);

    return true;
}
