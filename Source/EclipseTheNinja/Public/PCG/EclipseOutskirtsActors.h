// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCG/EclipsePCGBoxVolume.h"
#include "PCG/EclipsePCGLineActor.h"
#include "EclipseOutskirtsActors.generated.h"

class UEclipseTownHouseSet;

// ───────────────────────── 볼륨 ─────────────────────────

/**
 * 숲(PCG_Forest). 볼륨 안을 격자로 나눠 노이즈로 덩어리진 단풍·대나무 숲을 만든다.
 * 성벽 선 주변, "표지 볼륨"(플레이 면·마을) 안은 자동으로 비운다. 나무 높이는 볼륨과 상관없이 월드 Z 0.
 */
UCLASS(DisplayName = "Eclipse 숲 볼륨")
class ECLIPSETHENINJA_API AEclipseForestVolume : public AEclipsePCGBoxVolume
{
    GENERATED_BODY()

public:
    AEclipseForestVolume(const FObjectInitializer& ObjectInitializer);

protected:
    // 격자 간격·범위를 볼륨 크기에서 계산해 넣는다
    virtual void PushComputedGraphParameters() override;

    // 무작위 시드. 숫자만 바꾸면 같은 밀도로 나무 위치가 달라진다
    UPROPERTY(EditAnywhere, Category = "숲", meta = (PCGParam, DisplayName = "시드"))
    int32 Seed = 4242;

    // 나무 후보 간격(cm). 클수록 나무 수가 줄고 숲이 성기다. 700 → 1300으로 바꾸면 나무가 약 1/3로 준다
    UPROPERTY(EditAnywhere, Category = "숲|밀도", meta = (DisplayName = "나무 간격", Units = "cm", ClampMin = "200"))
    double CellSpacing = 700.0;

    // 0~1. 높일수록 빈 땅이 넓어지고 나무가 덩어리로 뭉친다. 0이면 빈틈 없이 빽빽, 0.7 이상이면 군데군데 작은 숲만 남는다
    UPROPERTY(EditAnywhere, Category = "숲|밀도", meta = (PCGParam, DisplayName = "빈 땅 비율(노이즈 문턱)", ClampMin = "0", ClampMax = "1"))
    double NoiseThreshold = 0.42;

    // 0~1. 나무 중 단풍나무 비율. 나머지는 대나무 다발. 1이면 대나무가 없다
    UPROPERTY(EditAnywhere, Category = "숲|구성", meta = (PCGParam, DisplayName = "단풍나무 비율", ClampMin = "0", ClampMax = "1"))
    double MapleRatio = 0.45;

    // 단풍나무 크기 배율 최솟값. 원경일수록 크게(4~5.5) 하면 멀리서도 숲 윤곽이 보인다
    UPROPERTY(EditAnywhere, Category = "숲|구성", meta = (PCGParam, DisplayName = "단풍 크기 최소", ClampMin = "0.1"))
    double MapleScaleMin = 1.5;

    // 단풍나무 크기 배율 최댓값
    UPROPERTY(EditAnywhere, Category = "숲|구성", meta = (PCGParam, DisplayName = "단풍 크기 최대", ClampMin = "0.1"))
    double MapleScaleMax = 2.5;

    // 0~1. 대나무 자리에서 실제로 대나무를 세울 비율. 낮추면 대나무 다발이 듬성해진다
    UPROPERTY(EditAnywhere, Category = "숲|구성", meta = (PCGParam, DisplayName = "대나무 밀도", ClampMin = "0", ClampMax = "1"))
    double BambooKeepRatio = 0.4;

    // 대나무 크기 최솟값. X·Y = 굵기, Z = 높이 배율
    UPROPERTY(EditAnywhere, Category = "숲|구성", meta = (PCGParam, DisplayName = "대나무 크기 최소"))
    FVector BambooScaleMin = FVector(1.5, 1.5, 3.0);

    // 대나무 크기 최댓값. X·Y = 굵기, Z = 높이 배율
    UPROPERTY(EditAnywhere, Category = "숲|구성", meta = (PCGParam, DisplayName = "대나무 크기 최대"))
    FVector BambooScaleMax = FVector(2.2, 2.2, 5.0);

    // 성벽 선 양옆으로 나무를 비우는 폭(cm). 나무가 성벽을 가리면 키운다
    UPROPERTY(EditAnywhere, Category = "숲|비우기", meta = (PCGParam, DisplayName = "성벽 주변 비우는 폭", Units = "cm", ClampMin = "0"))
    double WallBandHalfWidth = 1200.0;

    // 성벽이 닫힌 고리(위에서 본 성곽)일 때만 켠다. 켜면 성벽 안쪽 전체를 비운다. 사이드뷰의 열린 성벽에서 켜면 엉뚱한 곳이 빈다
    UPROPERTY(EditAnywhere, Category = "숲|비우기", meta = (PCGParam, DisplayName = "닫힌 성곽 안 비우기"))
    bool ClearCastleInterior = false;

    // 위 옵션이 켜졌을 때 성벽 바깥으로 더 비우는 거리(cm)
    UPROPERTY(EditAnywhere, Category = "숲|비우기", meta = (PCGParam, DisplayName = "성곽 바깥 여유", Units = "cm", ClampMin = "0", EditCondition = "ClearCastleInterior"))
    double CastleClearance = 1500.0;
};

/**
 * 원경 실루엣(PCG_FarSilhouette). 아주 먼 곳의 천수각·망루·마을 지붕 무리·나무를 그림자 없이 놓는다.
 * 볼륨 높이(Z)는 지평선 높이를 정한다(예시 레벨은 중심 Z 2500).
 */
UCLASS(DisplayName = "Eclipse 원경 실루엣 볼륨")
class ECLIPSETHENINJA_API AEclipseFarSilhouetteVolume : public AEclipsePCGBoxVolume
{
    GENERATED_BODY()

public:
    AEclipseFarSilhouetteVolume(const FObjectInitializer& ObjectInitializer);

protected:
    // 천수각 범위·나무 격자·후보 범위를 볼륨 크기에서 계산해 넣는다
    virtual void PushComputedGraphParameters() override;

    // 무작위 시드. 천수각·망루·지붕 무리 위치가 바뀐다
    UPROPERTY(EditAnywhere, Category = "원경", meta = (PCGParam, DisplayName = "시드"))
    int32 Seed = 0;

    // 천수각(성 본채) 수. 1~2 권장. 많으면 성이 여러 채로 보여 어색하다
    UPROPERTY(EditAnywhere, Category = "원경|천수각", meta = (PCGParam, DisplayName = "천수각 수", ClampMin = "0"))
    int32 TenshuCount = 1;

    // 천수각이 설 수 있는 범위: 볼륨 중심에서 좌우로 이 거리 안(cm). 플레이 구간 길이의 절반 정도로 두면 플레이 중 항상 보인다
    UPROPERTY(EditAnywhere, Category = "원경|천수각", meta = (DisplayName = "천수각 좌우 범위", Units = "cm", ClampMin = "0"))
    double TenshuRangeX = 9000.0;

    // 천수각 크기 배율 최솟값
    UPROPERTY(EditAnywhere, Category = "원경|천수각", meta = (PCGParam, DisplayName = "천수각 크기 최소", ClampMin = "0.1"))
    double TenshuScaleMin = 1.0;

    // 천수각 크기 배율 최댓값
    UPROPERTY(EditAnywhere, Category = "원경|천수각", meta = (PCGParam, DisplayName = "천수각 크기 최대", ClampMin = "0.1"))
    double TenshuScaleMax = 1.3;

    // 먼 망루 수
    UPROPERTY(EditAnywhere, Category = "원경|건물", meta = (PCGParam, DisplayName = "망루 수", ClampMin = "0"))
    int32 YaguraCount = 3;

    // 먼 마을 지붕 무리 수. 늘리면 지평선에 지붕이 많아진다
    UPROPERTY(EditAnywhere, Category = "원경|건물", meta = (PCGParam, DisplayName = "지붕 무리 수", ClampMin = "0"))
    int32 RoofClusterCount = 6;

    // 먼 나무 간격(cm). 클수록 나무가 적다. 에디터가 느리면 먼저 키운다
    UPROPERTY(EditAnywhere, Category = "원경|나무", meta = (DisplayName = "나무 간격", Units = "cm", ClampMin = "300"))
    double TreeSpacing = 1800.0;

    // 0~1. 높일수록 나무가 적고 덩어리진다
    UPROPERTY(EditAnywhere, Category = "원경|나무", meta = (PCGParam, DisplayName = "빈 땅 비율(노이즈 문턱)", ClampMin = "0", ClampMax = "1"))
    double TreeNoiseThreshold = 0.45;

    // 먼 나무 크기 배율 최솟값
    UPROPERTY(EditAnywhere, Category = "원경|나무", meta = (PCGParam, DisplayName = "나무 크기 최소", ClampMin = "0.1"))
    double TreeScaleMin = 3.5;

    // 먼 나무 크기 배율 최댓값
    UPROPERTY(EditAnywhere, Category = "원경|나무", meta = (PCGParam, DisplayName = "나무 크기 최대", ClampMin = "0.1"))
    double TreeScaleMax = 5.0;
};

/**
 * 전경(PCG_Foreground). 카메라와 플레이 면 사이에 나무·가로등을 아주 드물게 세워 깊이감을 준다.
 * 화면을 가리므로 개수를 적게 둔다. 볼륨은 Y +300 ~ +680 사이에 얇게 둔다.
 */
UCLASS(DisplayName = "Eclipse 전경 볼륨")
class ECLIPSETHENINJA_API AEclipseForegroundVolume : public AEclipsePCGBoxVolume
{
    GENERATED_BODY()

public:
    AEclipseForegroundVolume(const FObjectInitializer& ObjectInitializer);

protected:
    // 후보 범위(볼륨 가로)를 넣는다
    virtual void PushComputedGraphParameters() override;

    // 무작위 시드. 전경 오브젝트 위치가 바뀐다
    UPROPERTY(EditAnywhere, Category = "전경", meta = (PCGParam, DisplayName = "시드"))
    int32 Seed = 0;

    // 0~1. 후보 중 남길 비율. 높이면 개수가 늘지만 최대 개수를 넘지 않는다
    UPROPERTY(EditAnywhere, Category = "전경", meta = (PCGParam, DisplayName = "밀도", ClampMin = "0", ClampMax = "1"))
    double Density = 0.12;

    // 오브젝트 사이 최소 가로 간격(cm). 플레이 화면 폭은 약 1270cm이므로 1500 이상이면 한 화면에 하나 이하
    UPROPERTY(EditAnywhere, Category = "전경", meta = (PCGParam, DisplayName = "최소 간격", Units = "cm", ClampMin = "0"))
    double MinSpacing = 1500.0;

    // 볼륨 전체에서 놓을 최대 개수. 시야를 가리므로 3~5 권장
    UPROPERTY(EditAnywhere, Category = "전경", meta = (PCGParam, DisplayName = "최대 개수", ClampMin = "0"))
    int32 MaxCount = 4;

    // 0~1. 나무 비율. 나머지 자리는 가로등
    UPROPERTY(EditAnywhere, Category = "전경", meta = (PCGParam, DisplayName = "나무 비율", ClampMin = "0", ClampMax = "1"))
    double TreeRatio = 0.5;

    // 전경 나무 크기 배율 최솟값. 화면 위로 잘릴 만큼 크게 둔다
    UPROPERTY(EditAnywhere, Category = "전경", meta = (PCGParam, DisplayName = "나무 크기 최소", ClampMin = "0.1"))
    double TreeScaleMin = 3.0;

    // 전경 나무 크기 배율 최댓값
    UPROPERTY(EditAnywhere, Category = "전경", meta = (PCGParam, DisplayName = "나무 크기 최대", ClampMin = "0.1"))
    double TreeScaleMax = 4.0;
};

// 표지 볼륨 종류. 그래프 없이 태그만 달아 다른 PCG(숲)가 그 안을 비우게 한다
UENUM(BlueprintType)
enum class EEclipsePCGMarkerKind : uint8
{
    // 캐릭터가 다니는 플레이 면. 태그 PCG_Exclusion
    PlayArea UMETA(DisplayName = "플레이 면 (숲 비움)"),
    // 마을 영역. 태그 PCG_Village
    Village UMETA(DisplayName = "마을 (숲 비움)")
};

/**
 * 표지 볼륨. 생성물은 없고, 숲이 이 볼륨 안을 비운다.
 */
UCLASS(DisplayName = "Eclipse 표지 볼륨 (숲 비움)")
class ECLIPSETHENINJA_API AEclipsePCGMarkerVolume : public AEclipsePCGBoxVolume
{
    GENERATED_BODY()

public:
    AEclipsePCGMarkerVolume(const FObjectInitializer& ObjectInitializer);

    // 종류에 맞는 표지 태그를 붙인다
    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    // 표지 종류. 바꾸면 태그가 자동으로 바뀐다
    UPROPERTY(EditAnywhere, Category = "표지", meta = (DisplayName = "종류"))
    EEclipsePCGMarkerKind Kind = EEclipsePCGMarkerKind::PlayArea;
};

// ───────────────────────── 선 ─────────────────────────

/**
 * 마을 집 줄(PCG_TownRow). 선을 따라 집 블럭(붙은 집 묶음)과 골목을 번갈아 놓는다.
 * 집 정면은 카메라(+Y) 쪽을 본다. 앞줄(Y −900 근처)은 1층 위주, 뒷줄(Y −2200 근처)은 2층 위주로 두 줄을 겹친다.
 */
UCLASS(DisplayName = "Eclipse 마을 집 줄")
class ECLIPSETHENINJA_API AEclipseTownRowActor : public AEclipsePCGLineActor
{
    GENERATED_BODY()

public:
    AEclipseTownRowActor();

protected:
    // 정면 방향(카메라 쪽)을 고정한다
    virtual void PushComputedGraphParameters() override;

    // 무작위 시드. 집 종류·블럭 크기·골목 폭이 바뀐다. 두 줄을 쓸 때는 줄마다 다르게
    UPROPERTY(EditAnywhere, Category = "마을", meta = (PCGParam, DisplayName = "시드"))
    int32 Seed = 0;

    // 집 후보 목록(집 세트 데이터 에셋). 다른 세트를 넣으면 같은 배치 규칙으로 다른 거리가 된다
    UPROPERTY(EditAnywhere, Category = "마을", meta = (PCGParam, DisplayName = "집 세트"))
    TSoftObjectPtr<UEclipseTownHouseSet> HouseSet;

    // 선 시작에서 첫 블럭까지 비우는 거리(cm). 앞줄과 뒷줄의 골목 위치를 어긋나게 할 때 뒷줄에 1000~2000
    UPROPERTY(EditAnywhere, Category = "마을|블럭", meta = (PCGParam, DisplayName = "첫 블럭 시작 거리", Units = "cm", ClampMin = "1"))
    double RowOffsetX = 1.0;

    // 블럭 하나에 붙어 있는 집 수 최솟값. 키우면 골목이 줄고 집이 길게 이어진다
    UPROPERTY(EditAnywhere, Category = "마을|블럭", meta = (PCGParam, DisplayName = "블럭당 집 수 최소", ClampMin = "1"))
    int32 BlockHousesMin = 3;

    // 블럭 하나에 붙어 있는 집 수 최댓값
    UPROPERTY(EditAnywhere, Category = "마을|블럭", meta = (PCGParam, DisplayName = "블럭당 집 수 최대", ClampMin = "1"))
    int32 BlockHousesMax = 6;

    // 블럭 안 이웃 집끼리 겹치는 폭(cm). 키우면 집이 촘촘해진다(앞줄 60 권장). 너무 크면 지붕이 파묻힌다
    UPROPERTY(EditAnywhere, Category = "마을|블럭", meta = (PCGParam, DisplayName = "집 겹침", Units = "cm"))
    double HouseOverlap = 20.0;

    // 골목 폭 최솟값(cm)
    UPROPERTY(EditAnywhere, Category = "마을|골목", meta = (PCGParam, DisplayName = "골목 폭 최소", Units = "cm", ClampMin = "0"))
    double AlleyMin = 250.0;

    // 골목 폭 최댓값(cm). 키우면 집 사이가 벌어져 뒷줄·성벽이 더 보인다
    UPROPERTY(EditAnywhere, Category = "마을|골목", meta = (PCGParam, DisplayName = "골목 폭 최대", Units = "cm", ClampMin = "0"))
    double AlleyMax = 700.0;

    // 0~1. 골목에 흙담을 칠 비율
    UPROPERTY(EditAnywhere, Category = "마을|골목", meta = (PCGParam, DisplayName = "골목 흙담 비율", ClampMin = "0", ClampMax = "1"))
    double AlleyFenceRatio = 0.6;

    // 0~1. 골목에 소품 무리(통·수레 등)를 둘 비율
    UPROPERTY(EditAnywhere, Category = "마을|골목", meta = (PCGParam, DisplayName = "골목 소품 비율", ClampMin = "0", ClampMax = "1"))
    double AlleyPropDensity = 0.0;

    // 0~1. 블럭이 2층 집 블럭일 확률. 앞줄은 0으로 둔다(앞줄에 2층이 서면 뒤 레이어가 다 가려진다)
    UPROPERTY(EditAnywhere, Category = "마을|집 구성", meta = (PCGParam, DisplayName = "2층 블럭 비율", ClampMin = "0", ClampMax = "1"))
    double TwoStoryRatio = 0.5;

    // 0~1. 블럭이 나무지붕 블럭일 확률. 나무 블럭 다음은 항상 기와 블럭. 나무지붕 집이 튀면 낮춘다
    UPROPERTY(EditAnywhere, Category = "마을|집 구성", meta = (PCGParam, DisplayName = "나무지붕 블럭 비율", ClampMin = "0", ClampMax = "1"))
    double WoodRoofBlockRatio = 0.3;

    // 나무지붕 블럭의 집 수 최솟값
    UPROPERTY(EditAnywhere, Category = "마을|집 구성", meta = (PCGParam, DisplayName = "나무지붕 블럭 집 수 최소", ClampMin = "1"))
    int32 WoodBlockHousesMin = 1;

    // 나무지붕 블럭의 집 수 최댓값
    UPROPERTY(EditAnywhere, Category = "마을|집 구성", meta = (PCGParam, DisplayName = "나무지붕 블럭 집 수 최대", ClampMin = "1"))
    int32 WoodBlockHousesMax = 3;

    // 0~1. 집 앞 소품(통·화분 등)을 남길 비율
    UPROPERTY(EditAnywhere, Category = "마을|거리", meta = (PCGParam, DisplayName = "집 앞 소품 비율", ClampMin = "0", ClampMax = "1"))
    double PropDensity = 0.3;

    // 가로등 간격(cm). 골목 입구에는 세우지 않는다
    UPROPERTY(EditAnywhere, Category = "마을|거리", meta = (PCGParam, DisplayName = "가로등 간격", Units = "cm", ClampMin = "100"))
    double LampSpacing = 1800.0;

    // 집 앞에 흙길 타일을 깐다
    UPROPERTY(EditAnywhere, Category = "마을|거리", meta = (PCGParam, DisplayName = "집 앞 흙길"))
    bool PlaceStreet = true;
};

// 돌출부(망루 자리) 위치를 정하는 방식
UENUM(BlueprintType)
enum class EEclipseBastionPlacement : uint8
{
    // 개수만 정하면 길이에 고르게 나눈다
    Auto UMETA(DisplayName = "자동 (개수)"),
    // 시작점에서의 거리 목록으로 직접 정한다
    Manual UMETA(DisplayName = "직접 (위치 목록)")
};

/**
 * 성벽(PCG_CastleWall). 직선 석축 위에 흙담을 얹고, 돌출부마다 코너 석축과 망루를 세운다.
 *  - 액터는 바닥 높이(Z 0 등)에 둔다. 석축 윗면 높이는 성벽 높이 배율에서 자동으로 계산한다
 *  - 돌출부 = 성벽이 카메라 쪽으로 사각형으로 튀어나온 구간. 돌출부 하나에 망루 하나
 *  - 숲이 이 성벽을 찾을 수 있도록 태그 PCG_CastleWall이 자동으로 붙는다
 */
UCLASS(DisplayName = "Eclipse 성벽")
class ECLIPSETHENINJA_API AEclipseCastleWallActor : public AEclipsePCGLineActor
{
    GENERATED_BODY()

public:
    AEclipseCastleWallActor();

protected:
    // 직선 + 돌출부 사각형 점을 만든다(높이는 석축 윗면)
    virtual void BuildLinePoints(TArray<FVector>& OutPoints) const override;
    // 돌출부당 망루 하나가 되게 망루 간격 등을 넣는다
    virtual void PushComputedGraphParameters() override;

    // 돌출부 중심 위치(선 시작에서의 거리) 목록을 계산한다
    void ComputeBastionCenters(TArray<double>& OutCenters) const;

    // 무작위 시드. 자동 배치의 흔들림과 망루 모양 선택이 바뀐다
    UPROPERTY(EditAnywhere, Category = "성벽", meta = (PCGParam, DisplayName = "시드"))
    int32 Seed = 0;

    // 석축 높이 배율(1 = 400cm). 바꾸면 석축 윗면 높이가 400 × 이 값으로 자동 조정된다
    UPROPERTY(EditAnywhere, Category = "성벽", meta = (PCGParam, DisplayName = "성벽 높이 배율", ClampMin = "0.25"))
    double WallHeightScale = 1.5;

    // 석축 위에 흙담을 얹는다
    UPROPERTY(EditAnywhere, Category = "성벽|흙담", meta = (PCGParam, DisplayName = "흙담"))
    bool SpawnFence = true;

    // 흙담 위치 보정(cm, 진행 방향 기준). Y가 음수일수록 성 안쪽(뒤)으로 물러난다
    UPROPERTY(EditAnywhere, Category = "성벽|흙담", meta = (PCGParam, DisplayName = "흙담 위치 보정", EditCondition = "SpawnFence"))
    FVector FenceOffset = FVector(0.0, -50.0, 0.0);

    // 돌출부 위치를 정하는 방식
    UPROPERTY(EditAnywhere, Category = "성벽|돌출부·망루", meta = (DisplayName = "배치 방식"))
    EEclipseBastionPlacement BastionPlacement = EEclipseBastionPlacement::Auto;

    // 돌출부 수. 길이를 이 수로 나눈 칸마다 가운데 하나씩
    UPROPERTY(EditAnywhere, Category = "성벽|돌출부·망루", meta = (DisplayName = "돌출부 수", ClampMin = "0", EditCondition = "BastionPlacement == EEclipseBastionPlacement::Auto", EditConditionHides))
    int32 BastionCount = 2;

    // 0~1. 자동 위치를 칸 안에서 무작위로 흔드는 정도. 0이면 정확히 고른 간격
    UPROPERTY(EditAnywhere, Category = "성벽|돌출부·망루", meta = (DisplayName = "위치 흔들림", ClampMin = "0", ClampMax = "1", EditCondition = "BastionPlacement == EEclipseBastionPlacement::Auto", EditConditionHides))
    double BastionJitter = 0.3;

    // 돌출부 중심 위치 목록(선 시작에서의 거리, cm). 앞 돌출부와 (폭 + 200)cm 넘게 떨어져야 하고, 선 밖이면 건너뛴다
    UPROPERTY(EditAnywhere, Category = "성벽|돌출부·망루", meta = (DisplayName = "돌출부 위치 목록", EditCondition = "BastionPlacement == EEclipseBastionPlacement::Manual", EditConditionHides))
    TArray<double> BastionPositions;

    // 돌출부 가로 폭(cm). 망루가 약 1200cm라 1400 이상 권장
    UPROPERTY(EditAnywhere, Category = "성벽|돌출부·망루", meta = (DisplayName = "돌출부 폭", Units = "cm", ClampMin = "800"))
    double BastionWidth = 1600.0;

    // 돌출부가 카메라 쪽(+Y)으로 튀어나오는 거리(cm). 망루가 올라가려면 1100 이상
    UPROPERTY(EditAnywhere, Category = "성벽|돌출부·망루", meta = (DisplayName = "돌출 깊이", Units = "cm", ClampMin = "400"))
    double BastionDepth = 1200.0;

    // 돌출부 위에 망루를 세운다. 끄면 석축만 튀어나온다
    UPROPERTY(EditAnywhere, Category = "성벽|돌출부·망루", meta = (PCGParam, DisplayName = "망루"))
    bool SpawnYagura = true;
};
