// Fill out your copyright notice in the Description page of Project Settings.

#include "PCG/PCGEclipseTownRow.h"

#include "PCG/EclipseTownTypes.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGPolyLineData.h"
#include "Engine/StaticMesh.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Math/RandomStream.h"

namespace EclipseTownRow
{
    // 집 후보 하나를 실행용으로 풀어 둔 것
    struct FCandidate
    {
        FSoftObjectPath Mesh;
        bool bWood = false;
        int32 Stories = 1;
        float Weight = 1.0f;

        // 메시 바운드 X 폭
        double Width = 0.0;

        // 메시 바운드 가운데 X(피벗이 가운데가 아닐 때 보정)
        double CenterX = 0.0;
    };

    // 스플라인 위 한 구간(집 또는 골목)
    struct FModule
    {
        double Start = 0.0;
        double Size = 0.0;
        bool bHouse = false;
        int32 Candidate = INDEX_NONE;
        int32 BlockIndex = 0;
    };

    // 스플라인 전체 거리를 (구간 번호, 구간 안 거리)로 바꿔 위치를 구한다
    FTransform TransformAtDistance(const UPCGPolyLineData* Line, double Distance)
    {
        const int32 NumSegments = Line->GetNumSegments();
        double Remain = FMath::Max(0.0, Distance);

        for (int32 Segment = 0; Segment < NumSegments; ++Segment)
        {
            const double SegmentLength = Line->GetSegmentLength(Segment);
            const bool bLast = Segment == NumSegments - 1;

            if (Remain <= SegmentLength || bLast)
            {
                return Line->GetTransformAtDistance(Segment, FMath::Min(Remain, SegmentLength), /*bWorldSpace=*/true);
            }

            Remain -= SegmentLength;
        }

        return FTransform::Identity;
    }

    // 조건에 맞는 후보 중 가중치로 하나 고른다. 맞는 후보가 없으면 INDEX_NONE
    int32 PickCandidate(
        const TArray<FCandidate>& Candidates,
        const TArray<int32>& Pool,
        int32 Exclude,
        double MaxSize,
        double Overlap,
        FRandomStream& Stream
    )
    {
        float Total = 0.0f;
        TArray<int32, TInlineAllocator<16>> Allowed;

        for (const int32 Index : Pool)
        {
            const FCandidate& Candidate = Candidates[Index];
            const bool bFits = Candidate.Width - Overlap <= MaxSize;

            if (Index == Exclude || !bFits || Candidate.Weight <= 0.0f)
            {
                continue;
            }

            Allowed.Add(Index);
            Total += Candidate.Weight;
        }

        if (Allowed.IsEmpty())
        {
            return INDEX_NONE;
        }

        float Roll = static_cast<float>(Stream.FRandRange(0.0, Total));

        for (const int32 Index : Allowed)
        {
            Roll -= Candidates[Index].Weight;
            if (Roll <= 0.0f)
            {
                return Index;
            }
        }

        return Allowed.Last();
    }

    // 지붕 계열·층수에 맞는 후보 번호 목록. 없으면 층수만, 그래도 없으면 전체로 넓힌다
    TArray<int32> MakePool(const TArray<FCandidate>& Candidates, bool bWood, int32 Stories)
    {
        TArray<int32> Pool;

        for (int32 Index = 0; Index < Candidates.Num(); ++Index)
        {
            if (Candidates[Index].bWood == bWood && Candidates[Index].Stories == Stories)
            {
                Pool.Add(Index);
            }
        }

        if (Pool.IsEmpty())
        {
            for (int32 Index = 0; Index < Candidates.Num(); ++Index)
            {
                if (Candidates[Index].Stories == Stories)
                {
                    Pool.Add(Index);
                }
            }
        }

        if (Pool.IsEmpty())
        {
            for (int32 Index = 0; Index < Candidates.Num(); ++Index)
            {
                Pool.Add(Index);
            }
        }

        return Pool;
    }
}

TArray<FPCGPinProperties> UPCGEclipseTownRowSettings::InputPinProperties() const
{
    TArray<FPCGPinProperties> PinProperties;
    PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spline);
    return PinProperties;
}

TArray<FPCGPinProperties> UPCGEclipseTownRowSettings::OutputPinProperties() const
{
    TArray<FPCGPinProperties> PinProperties;
    PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
    return PinProperties;
}

FPCGElementPtr UPCGEclipseTownRowSettings::CreateElement() const
{
    return MakeShared<FPCGEclipseTownRowElement>();
}

bool FPCGEclipseTownRowElement::ExecuteInternal(FPCGContext* Context) const
{
    using namespace EclipseTownRow;

    const UPCGEclipseTownRowSettings* Settings = Context->GetInputSettings<UPCGEclipseTownRowSettings>();
    check(Settings);

    // 집 후보: 세트를 읽고 메시 바운드로 폭을 잰다
    const UEclipseTownHouseSet* HouseSet = Settings->HouseSet.LoadSynchronous();
    TArray<FCandidate> Candidates;

    if (HouseSet)
    {
        for (const FEclipseTownHouseEntry& Entry : HouseSet->Houses)
        {
            const UStaticMesh* Mesh = Entry.Mesh.LoadSynchronous();

            if (!Mesh)
            {
                continue;
            }

            const FBox Box = Mesh->GetBoundingBox();
            FCandidate& Candidate = Candidates.AddDefaulted_GetRef();
            Candidate.Mesh = Entry.Mesh.ToSoftObjectPath();
            Candidate.bWood = Entry.Roof == EEclipseTownRoof::Wood;
            Candidate.Stories = FMath::Clamp(Entry.Stories, 1, 2);
            Candidate.Weight = Entry.Weight;
            Candidate.CenterX = Box.GetCenter().X;
            Candidate.Width = Entry.WidthOverride > 0.0f
                ? static_cast<double>(Entry.WidthOverride)
                : Box.GetSize().X;
        }
    }

    if (Candidates.IsEmpty())
    {
        PCGE_LOG(Warning, GraphAndLog, NSLOCTEXT("PCGEclipseTownRow", "NoHouses", "House set is empty or not assigned."));
        return true;
    }

    const bool bHasWood = Candidates.ContainsByPredicate([](const FCandidate& Candidate) { return Candidate.bWood; });
    const double Overlap = Settings->HouseOverlap;
    const double AlleyMin = FMath::Max(0.0f, FMath::Min(Settings->AlleyMin, Settings->AlleyMax));
    const double AlleyMax = FMath::Max(Settings->AlleyMin, Settings->AlleyMax);
    const int32 BlockMin = FMath::Max(1, FMath::Min(Settings->BlockHousesMin, Settings->BlockHousesMax));
    const int32 BlockMax = FMath::Max(BlockMin, Settings->BlockHousesMax);
    const int32 WoodMin = FMath::Max(1, FMath::Min(Settings->WoodBlockHousesMin, Settings->WoodBlockHousesMax));
    const int32 WoodMax = FMath::Max(WoodMin, Settings->WoodBlockHousesMax);

    for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
    {
        const UPCGPolyLineData* Line = Cast<UPCGPolyLineData>(Input.Data);

        if (!Line || Line->GetNumSegments() <= 0)
        {
            continue;
        }

        const double Length = Line->GetLength();
        FRandomStream Stream(PCGHelpers::ComputeSeed(Settings->LayoutSeed, 9431));

        // 블럭과 골목 나누기: [시작 비움] 블럭, (골목, 블럭)* 순서. 남은 길이에 집이 안 들어가면 끝
        TArray<FModule> Modules;
        double Cursor = FMath::Max(0.0f, Settings->RowOffsetX);
        bool bPrevWood = false;
        bool bDone = false;
        int32 BlockIndex = 0;

        while (!bDone)
        {
            // 첫 블럭이 아니면 골목부터
            int32 AlleyModule = INDEX_NONE;

            if (BlockIndex > 0)
            {
                const double AlleyWidth = Stream.FRandRange(AlleyMin, AlleyMax);

                if (Cursor + AlleyWidth >= Length)
                {
                    break;
                }

                FModule& Alley = Modules.AddDefaulted_GetRef();
                Alley.Start = Cursor;
                Alley.Size = AlleyWidth;
                Alley.BlockIndex = BlockIndex;
                AlleyModule = Modules.Num() - 1;
                Cursor += AlleyWidth;
            }

            // 블럭 성격: 지붕 계열(나무 블럭은 연달아 나오지 않음), 층수, 집 수
            const bool bWood = bHasWood && !bPrevWood && Stream.FRand() < Settings->WoodRoofBlockRatio;
            const int32 Stories = Stream.FRand() < Settings->TwoStoryRatio ? 2 : 1;
            const int32 NumHouses = bWood
                ? Stream.RandRange(WoodMin, WoodMax)
                : Stream.RandRange(BlockMin, BlockMax);
            const TArray<int32> Pool = MakePool(Candidates, bWood, Stories);

            // 집 채우기: 바로 앞 집과 같은 메시는 피하고, 남은 길이에 맞는 후보만
            int32 Placed = 0;
            int32 Previous = INDEX_NONE;

            for (int32 House = 0; House < NumHouses; ++House)
            {
                const double Remain = Length - Cursor;
                int32 Pick = PickCandidate(Candidates, Pool, Previous, Remain, Overlap, Stream);

                if (Pick == INDEX_NONE && Previous != INDEX_NONE)
                {
                    Pick = PickCandidate(Candidates, Pool, INDEX_NONE, Remain, Overlap, Stream);
                }

                if (Pick == INDEX_NONE)
                {
                    bDone = true;
                    break;
                }

                FModule& Module = Modules.AddDefaulted_GetRef();
                Module.Start = Cursor;
                Module.Size = FMath::Max(1.0, Candidates[Pick].Width - Overlap);
                Module.bHouse = true;
                Module.Candidate = Pick;
                Module.BlockIndex = BlockIndex;
                Cursor += Module.Size;
                Previous = Pick;
                ++Placed;
            }

            // 집이 하나도 안 들어간 블럭이면 앞 골목도 지운다
            if (Placed == 0)
            {
                if (AlleyModule != INDEX_NONE)
                {
                    Modules.RemoveAt(AlleyModule);
                }

                break;
            }

            bPrevWood = bWood;
            ++BlockIndex;
        }

        // 포인트 쓰기: Subdivide Spline과 같은 위치·회전·바운드
        UPCGBasePointData* OutPoints = FPCGContext::NewPointData_AnyThread(Context);
        OutPoints->SetNumPoints(Modules.Num(), /*bInitializeValues=*/false);
        OutPoints->AllocateProperties(EPCGPointNativeProperties::All);

        UPCGMetadata* Metadata = OutPoints->MutableMetadata();
        FPCGMetadataAttribute<FSoftObjectPath>* MeshAttribute = Metadata->FindOrCreateAttribute<FSoftObjectPath>(
            TEXT("Mesh"),
            FSoftObjectPath(),
            /*bAllowsInterpolation=*/false,
            /*bOverrideParent=*/true
        );
        FPCGMetadataAttribute<double>* SizeAttribute = Metadata->FindOrCreateAttribute<double>(TEXT("Size"), 0.0);
        FPCGMetadataAttribute<int32>* CountAttribute = Metadata->FindOrCreateAttribute<int32>(TEXT("Count"), 0);
        FPCGMetadataAttribute<int32>* RoofAttribute = Metadata->FindOrCreateAttribute<int32>(TEXT("Roof"), 0);
        FPCGMetadataAttribute<int32>* StoriesAttribute = Metadata->FindOrCreateAttribute<int32>(TEXT("Stories"), 0);
        FPCGMetadataAttribute<int32>* BlockAttribute = Metadata->FindOrCreateAttribute<int32>(TEXT("BlockIndex"), 0);

        FPCGPointValueRanges OutRanges(OutPoints, /*bAllocate=*/false);

        for (int32 Index = 0; Index < Modules.Num(); ++Index)
        {
            const FModule& Module = Modules[Index];
            const FVector StartLocation = TransformAtDistance(Line, Module.Start).GetLocation();
            const FVector EndLocation = TransformAtDistance(Line, Module.Start + Module.Size).GetLocation();
            const FVector Direction = (EndLocation - StartLocation).GetSafeNormal();
            const FVector Up = TransformAtDistance(Line, Module.Start + Module.Size * 0.5).GetRotation().GetUpVector();
            const FRotator Rotation = FRotationMatrix::MakeFromXZ(Direction, Up).Rotator();
            FVector Location = (StartLocation + EndLocation) * 0.5;

            // 그래프가 집을 Yaw 180으로 돌리므로, 메시 바운드 가운데가 모듈 가운데에 오게 진행 방향으로 보정
            if (Module.bHouse)
            {
                Location += Direction * Candidates[Module.Candidate].CenterX;
            }

            const double HalfSize = Module.Size * 0.5;

            FPCGPoint Point(
                FTransform(Rotation, Location, FVector::OneVector),
                /*InDensity=*/1.0f,
                PCGHelpers::ComputeSeedFromPosition(Location)
            );
            Point.SetLocalBounds(FBox(FVector(-HalfSize, 0.0, 0.0), FVector(HalfSize, 1.0, 0.0)));
            Point.MetadataEntry = Metadata->AddEntry();

            SizeAttribute->SetValue(Point.MetadataEntry, Module.Size);
            BlockAttribute->SetValue(Point.MetadataEntry, Module.BlockIndex);

            if (Module.bHouse)
            {
                const FCandidate& Candidate = Candidates[Module.Candidate];
                MeshAttribute->SetValue(Point.MetadataEntry, Candidate.Mesh);
                CountAttribute->SetValue(Point.MetadataEntry, 1);
                RoofAttribute->SetValue(Point.MetadataEntry, Candidate.bWood ? 1 : 0);
                StoriesAttribute->SetValue(Point.MetadataEntry, Candidate.Stories);
            }

            OutRanges.SetFromPoint(Index, Point);
        }

        FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
        Output.Data = OutPoints;
        Output.Pin = PCGPinConstants::DefaultOutputLabel;
    }

    return true;
}
