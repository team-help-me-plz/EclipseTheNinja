// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCG/EclipsePCGBoxVolume.h"
#include "EclipseGroundVolume.generated.h"

class UStaticMesh;

// 바닥 타일 후보. 가운데 피벗, 윗면 Z=0인 평판 메시
USTRUCT(BlueprintType)
struct FEclipseGroundTileEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (DisplayName = "메시"))
    TSoftObjectPtr<UStaticMesh> Mesh;

    // 다른 후보 대비 뽑힐 비중
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (DisplayName = "비중", ClampMin = "0"))
    float Weight = 1.0f;
};

// 흩뿌림 후보(덤불·돌 등)
USTRUCT(BlueprintType)
struct FEclipseGroundScatterEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (DisplayName = "메시"))
    TSoftObjectPtr<UStaticMesh> Mesh;

    // 다른 후보 대비 뽑힐 비중
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (DisplayName = "비중", ClampMin = "0"))
    float Weight = 1.0f;

    // 크기 배율 최솟값
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (DisplayName = "크기 최소", ClampMin = "0.01"))
    float ScaleMin = 1.0f;

    // 크기 배율 최댓값
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (DisplayName = "크기 최대", ClampMin = "0.01"))
    float ScaleMax = 1.0f;
};

// 바닥 생성 설정. 볼륨 디테일 패널에서 고친다
USTRUCT(BlueprintType)
struct FEclipseGroundParams
{
    GENERATED_BODY()

    // 무작위 시드. 타일 종류·회전, 흩뿌림 위치가 바뀐다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "바닥", meta = (DisplayName = "시드"))
    int32 Seed = 0;

    // 바닥 타일 후보. 여러 개면 비중대로 섞는다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "바닥|타일", meta = (DisplayName = "타일 메시"))
    TArray<FEclipseGroundTileEntry> Tiles;

    // 타일 한 장 크기(cm). 0이면 첫 타일 메시의 가로 폭을 쓴다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "바닥|타일", meta = (DisplayName = "타일 크기", Units = "cm", ClampMin = "0"))
    float TileSize = 0.0f;

    // 볼륨에 딱 맞도록 타일을 살짝 늘이거나 줄인다. 끄면 원래 크기로 깔고 끝이 볼륨 밖으로 조금 나간다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "바닥|타일", meta = (DisplayName = "볼륨에 맞춰 늘이기"))
    bool bFitTilesToVolume = true;

    // 타일마다 90도 단위로 무작위 회전해 반복 무늬를 숨긴다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "바닥|타일", meta = (DisplayName = "타일 무작위 회전"))
    bool bRandomTileRotation = true;

    // 타일 윗면 높이 보정(cm). 0이면 윗면이 볼륨 윗면에 맞는다. 게임플레이 바닥과 겹쳐 깜빡이면 -1~-2
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "바닥|타일", meta = (DisplayName = "높이 보정", Units = "cm"))
    float SurfaceOffset = 0.0f;

    // 흩뿌릴 후보(덤불·돌 등)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "바닥|흩뿌림", meta = (DisplayName = "흩뿌림 메시"))
    TArray<FEclipseGroundScatterEntry> Scatter;

    // 10m × 10m당 흩뿌림 개수. 0(기본)이면 흙 타일만 깐다. 6~8이면 군데군데 덤불, 20이면 꽤 빽빽하다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "바닥|흩뿌림", meta = (DisplayName = "밀도 (개/100㎡)", ClampMin = "0"))
    float ScatterDensity = 0.0f;

    // 0~1. 높이면 흩뿌림이 무리 지어 모이고 빈 땅이 넓어진다. 0이면 고르게 퍼진다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "바닥|흩뿌림", meta = (DisplayName = "뭉침", ClampMin = "0", ClampMax = "1"))
    float ScatterClumping = 0.5f;

    // 흩뿌림끼리 최소 간격(cm). 겹쳐 보이면 키운다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "바닥|흩뿌림", meta = (DisplayName = "최소 간격", Units = "cm", ClampMin = "0"))
    float ScatterMinSpacing = 60.0f;

    // 플레이 면 표지 볼륨(태그 PCG_Exclusion) 안과 그 앞(카메라 쪽)에는 흩뿌리지 않는다. 발밑이 지저분해지거나 덤불이 화면을 가리지 않게
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "바닥|흩뿌림", meta = (DisplayName = "플레이 면과 앞쪽 피하기"))
    bool bAvoidPlayArea = true;
};

/**
 * 야외 배경 바닥(PCG_Ground). 볼륨 크기만큼 흙 타일을 깐다. 흩뿌림 밀도를 올리면 덤불·돌도 흩뿌린다(기본 끔).
 *  - 바닥 윗면 높이 = 볼륨 윗면. 볼륨을 바닥 아래로 얇게 두면 된다
 *  - 콜리전·내비 없음(배경 전용). 캐릭터가 밟는 바닥은 따로 만든다
 * 값은 그래프 노드 Eclipse Ground가 이 액터에서 직접 읽는다.
 */
UCLASS(DisplayName = "Eclipse 바닥 볼륨")
class ECLIPSETHENINJA_API AEclipseGroundVolume : public AEclipsePCGBoxVolume
{
    GENERATED_BODY()

public:
    AEclipseGroundVolume(const FObjectInitializer& ObjectInitializer);

    // 노드가 읽는 바닥 설정
    const FEclipseGroundParams& GetGroundParams() const { return this->Params; }

protected:
    virtual bool ReadsActorValuesDirectly() const override { return true; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "바닥", meta = (ShowOnlyInnerProperties))
    FEclipseGroundParams Params;
};
