// Fill out your copyright notice in the Description page of Project Settings.

#include "PCG/EclipseOutskirtsActors.h"

#include "EclipsePCGParams.h"
#include "PCG/EclipseTownTypes.h"
#include "PCGGraph.h"
#include "Math/RandomStream.h"

namespace EclipseOutskirts
{
    const FName ExclusionTag = TEXT("PCG_Exclusion");
    const FName VillageTag = TEXT("PCG_Village");
    const FName CastleWallTag = TEXT("PCG_CastleWall");

    // 석축 프리팹 한 단 높이(cm). 석축 피벗이 윗면이라 선 높이 = 이 값 × 높이 배율
    const double WallUnitHeight = 400.0;

    // 선 양 끝에 남길 최소 직선 길이(cm). 석축 한 조각
    const double MinStraight = 400.0;

    // /Game/PCG/Graphs/ 아래 그래프 경로
    TSoftObjectPtr<UPCGGraphInterface> GraphPath(const TCHAR* AssetName)
    {
        const FString Path = FString::Printf(
            TEXT("/Game/PCG/Graphs/%s.%s"),
            AssetName,
            AssetName
        );

        return TSoftObjectPtr<UPCGGraphInterface>(FSoftObjectPath(Path));
    }
}

// ───────────────────────── 숲 ─────────────────────────

AEclipseForestVolume::AEclipseForestVolume(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    this->DefaultGraph = EclipseOutskirts::GraphPath(TEXT("PCG_Forest"));
    this->Size = FVector(20000.0, 3600.0, 400.0);
}

void AEclipseForestVolume::PushComputedGraphParameters()
{
    // 격자 간격은 X·Y만 바꾸고 Z는 그래프 기본값(100) 유지
    EclipsePCGParams::SetVector(
        this->PCGComponent,
        TEXT("CellSize"),
        FVector(this->CellSpacing, this->CellSpacing, 100.0)
    );

    // 후보 격자는 볼륨보다 한 칸 크게(볼륨 밖은 그래프가 잘라낸다)
    const FVector Half = this->Size * 0.5;
    EclipsePCGParams::SetVector(
        this->PCGComponent,
        TEXT("GridExtents"),
        FVector(Half.X + this->CellSpacing, Half.Y + this->CellSpacing, 0.0)
    );
}

// ───────────────────────── 원경 ─────────────────────────

AEclipseFarSilhouetteVolume::AEclipseFarSilhouetteVolume(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    this->DefaultGraph = EclipseOutskirts::GraphPath(TEXT("PCG_FarSilhouette"));
    this->Size = FVector(60000.0, 20000.0, 400.0);
}

void AEclipseFarSilhouetteVolume::PushComputedGraphParameters()
{
    const FVector Half = this->Size * 0.5;

    // 천수각 후보 범위: 좌우는 사용자 값, 앞뒤는 볼륨 깊이 안(최대 6000)
    EclipsePCGParams::SetVector(
        this->PCGComponent,
        TEXT("TenshuZone"),
        FVector(this->TenshuRangeX, FMath::Min(6000.0, Half.Y), 0.0)
    );

    EclipsePCGParams::SetVector(
        this->PCGComponent,
        TEXT("TreeCellSize"),
        FVector(this->TreeSpacing, this->TreeSpacing, 100.0)
    );

    EclipsePCGParams::SetVector(
        this->PCGComponent,
        TEXT("Extents"),
        FVector(Half.X + this->TreeSpacing, Half.Y + this->TreeSpacing, 0.0)
    );
}

// ───────────────────────── 전경 ─────────────────────────

AEclipseForegroundVolume::AEclipseForegroundVolume(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    this->DefaultGraph = EclipseOutskirts::GraphPath(TEXT("PCG_Foreground"));
    this->Size = FVector(30000.0, 380.0, 400.0);
}

void AEclipseForegroundVolume::PushComputedGraphParameters()
{
    // 후보는 볼륨 가로 범위의 한 줄(Y 1)
    EclipsePCGParams::SetVector(
        this->PCGComponent,
        TEXT("Extents"),
        FVector(this->Size.X * 0.5, 1.0, 0.0)
    );
}

// ───────────────────────── 표지 ─────────────────────────

AEclipsePCGMarkerVolume::AEclipsePCGMarkerVolume(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    this->Size = FVector(20000.0, 600.0, 1000.0);
}

void AEclipsePCGMarkerVolume::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // 종류에 맞는 태그 하나만 남긴다(사용자가 단 다른 태그는 유지)
    const FName WantedTag = this->Kind == EEclipsePCGMarkerKind::Village
        ? EclipseOutskirts::VillageTag
        : EclipseOutskirts::ExclusionTag;

    this->Tags.Remove(EclipseOutskirts::ExclusionTag);
    this->Tags.Remove(EclipseOutskirts::VillageTag);
    this->Tags.Add(WantedTag);
}

// ───────────────────────── 마을 집 줄 ─────────────────────────

AEclipseTownRowActor::AEclipseTownRowActor()
{
    this->DefaultGraph = EclipseOutskirts::GraphPath(TEXT("PCG_TownRow"));
    this->HouseSet = TSoftObjectPtr<UEclipseTownHouseSet>(
        FSoftObjectPath(TEXT("/Game/PCG/Data/DA_TownHouses_Default.DA_TownHouses_Default"))
    );
}

void AEclipseTownRowActor::PushComputedGraphParameters()
{
    // 선은 항상 +X로 만들므로 정면은 진행 방향 오른쪽(카메라 쪽)
    EclipsePCGParams::SetNumber(this->PCGComponent, TEXT("FacingSide"), 1.0);
}

// ───────────────────────── 성벽 ─────────────────────────

AEclipseCastleWallActor::AEclipseCastleWallActor()
{
    this->DefaultGraph = EclipseOutskirts::GraphPath(TEXT("PCG_CastleWall"));
    this->Tags.Add(EclipseOutskirts::CastleWallTag);
}

void AEclipseCastleWallActor::ComputeBastionCenters(TArray<double>& OutCenters) const
{
    TArray<double> Candidates;

    if (this->BastionPlacement == EEclipseBastionPlacement::Manual)
    {
        Candidates = this->BastionPositions;
        Candidates.Sort();
    }
    else if (this->BastionCount > 0)
    {
        // 길이를 개수만큼 칸으로 나누고 칸 가운데에서 흔든다
        FRandomStream Stream(this->Seed);
        const double CellLength = this->Length / this->BastionCount;
        const double MaxShift = FMath::Max(0.0, (CellLength - this->BastionWidth) * 0.5) * this->BastionJitter;

        for (int32 Index = 0; Index < this->BastionCount; ++Index)
        {
            const double Center = CellLength * (Index + 0.5);
            Candidates.Add(Center + Stream.FRandRange(-MaxShift, MaxShift));
        }
    }

    // 선 밖으로 나가거나 앞 돌출부와 가까운 것은 건너뛴다.
    // 이웃 돌출부의 모서리끼리도 망루 간격 거르기(폭 + 100)에 걸리지 않도록 그보다 넓게 띄운다
    const double HalfWidth = this->BastionWidth * 0.5;
    const double MinGap = this->BastionWidth + 200.0;
    double PrevEnd = -MinGap;

    for (const double Center : Candidates)
    {
        const double Start = Center - HalfWidth;
        const double End = Center + HalfWidth;
        const bool bFits = Start >= FMath::Max(PrevEnd + MinGap, EclipseOutskirts::MinStraight)
            && End <= this->Length - EclipseOutskirts::MinStraight;

        if (!bFits)
        {
            continue;
        }

        OutCenters.Add(Center);
        PrevEnd = End;
    }
}

void AEclipseCastleWallActor::BuildLinePoints(TArray<FVector>& OutPoints) const
{
    // 석축 피벗이 윗면이므로 선을 석축 높이만큼 올린다 → 액터는 바닥에 둔다
    const double Z = EclipseOutskirts::WallUnitHeight * this->WallHeightScale;
    const double HalfWidth = this->BastionWidth * 0.5;

    TArray<double> Centers;
    this->ComputeBastionCenters(Centers);

    // 돌출부마다 사각형으로 튀어나온다. 앞쪽 두 모서리가 볼록 모서리가 되어 그래프가 코너 석축·망루를 세운다
    OutPoints.Add(FVector(0.0, 0.0, Z));
    for (const double Center : Centers)
    {
        OutPoints.Add(FVector(Center - HalfWidth, 0.0, Z));
        OutPoints.Add(FVector(Center - HalfWidth, this->BastionDepth, Z));
        OutPoints.Add(FVector(Center + HalfWidth, this->BastionDepth, Z));
        OutPoints.Add(FVector(Center + HalfWidth, 0.0, Z));
    }
    OutPoints.Add(FVector(this->Length, 0.0, Z));
}

void AEclipseCastleWallActor::PushComputedGraphParameters()
{
    // 망루는 돌출부의 볼록 모서리 두 곳 중 하나에만 선다(두 모서리 간격 = 돌출부 폭이므로 그보다 크게 거른다)
    EclipsePCGParams::SetNumber(this->PCGComponent, TEXT("YaguraRatio"), 1.0);
    EclipsePCGParams::SetNumber(this->PCGComponent, TEXT("YaguraMinSpacing"), this->BastionWidth + 100.0);
    EclipsePCGParams::SetNumber(this->PCGComponent, TEXT("CornerMinTurn"), 45.0);
    EclipsePCGParams::SetBool(this->PCGComponent, TEXT("FlipOutward"), false);
}
