// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EclipseInteriorTypes.generated.h"

class UStaticMesh;

// 성 내부 방 테마. 테마마다 PCG 그래프가 하나씩 있다
UENUM(BlueprintType)
enum class EEclipseInteriorTheme : uint8
{
    Storehouse UMETA(DisplayName = "창고"),
    Corridor UMETA(DisplayName = "장지 복도"),
    Tatami UMETA(DisplayName = "다다미 방"),
    Underground UMETA(DisplayName = "지하 (석축)")
};

// 변형 칸(격자창·열린 장지·족자)을 고르는 방식
UENUM(BlueprintType)
enum class EEclipseInteriorVariantMode : uint8
{
    // N칸마다 변형. 같은 값이면 항상 같은 결과
    Rule UMETA(DisplayName = "규칙 (N칸마다)"),
    // 비율만큼 Seed로 무작위 선택
    Ratio UMETA(DisplayName = "비율 + Seed")
};

// 가중치가 있는 메시 후보. Mesh가 비어 있으면 "아무것도 두지 않음" 후보
USTRUCT(BlueprintType)
struct FEclipseInteriorMeshEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior")
    TSoftObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior", meta = (ClampMin = "0"))
    float Weight = 1.0f;
};

// 바닥 소품 후보. Scale은 기본 크기 배율
USTRUCT(BlueprintType)
struct FEclipseInteriorPropEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior")
    TSoftObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior", meta = (ClampMin = "0"))
    float Weight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior", meta = (ClampMin = "0.01"))
    float Scale = 1.0f;
};

// 방 하나의 배치 파라미터. 볼륨 액터 디테일 패널에서 고친다
USTRUCT(BlueprintType)
struct FEclipseInteriorRoomParams
{
    GENERATED_BODY()

    // 비율 모드 변형 칸, 칸별 후보 선택, 소품 위치·종류
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior")
    int32 Seed = 0;

    // 칸(기둥 사이) 목표 폭. 실제 폭은 볼륨 길이에 맞게 늘거나 줄어든다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior", meta = (ClampMin = "100"))
    float BayWidth = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Variant")
    EEclipseInteriorVariantMode VariantMode = EEclipseInteriorVariantMode::Rule;

    // 규칙 모드: N칸마다 변형 칸 (0이면 변형 없음)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Variant", meta = (ClampMin = "0", EditCondition = "VariantMode == EEclipseInteriorVariantMode::Rule"))
    int32 VariantEvery = 3;

    // 규칙 모드: 패턴 시작 칸을 민다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Variant", meta = (EditCondition = "VariantMode == EEclipseInteriorVariantMode::Rule"))
    int32 BayOffset = 0;

    // 비율 모드: 변형 칸 비율 (0~1)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Variant", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "VariantMode == EEclipseInteriorVariantMode::Ratio"))
    float VariantRatio = 0.3f;

    // 양 끝 칸은 항상 기본 칸
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Variant")
    bool bKeepEndBaysPlain = true;

    // 뒷벽과 플레이 면 사이의 중간 층(기둥 줄·장지)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Layers")
    bool bSpawnMidLayer = true;

    // 뒷벽에서 중간 층까지 거리(cm). 볼륨 깊이가 이 값 + 50 이상일 때만 중간 층이 생긴다(기본 450 → 깊이 500, Y 스케일 2.5부터)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Layers", meta = (ClampMin = "100", EditCondition = "bSpawnMidLayer"))
    float MidLayerDistance = 450.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Layers")
    bool bSpawnFloor = true;

    // 위층이 보이는 단면이면 끈다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Layers")
    bool bSpawnCeiling = true;

    // 바닥 앞 가장자리에서 아래로 내리는 판벽 깊이(cm). 가시 구덩이처럼 바닥보다 낮은 곳의 뒤를 막는다. 0이면 없음
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Layers", meta = (ClampMin = "0"))
    float FrontSkirtDepth = 0.0f;

    // 방 왼쪽(-X) 끝에 옆벽. 이어 붙인 볼륨 사이에서는 끈다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Layers")
    bool bEndWallLeft = false;

    // 방 오른쪽(+X) 끝에 옆벽
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Layers")
    bool bEndWallRight = false;

    // 뒷벽 뒤·천장 위에 보이지 않는 그림자 차단 상자(한쪽 면 메시로 새는 햇빛을 막음)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Layers")
    bool bShadowBlocker = true;

    // 칸마다 소품 자리에 소품을 둘 확률 (0이면 소품 없음)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior|Props", meta = (ClampMin = "0", ClampMax = "1"))
    float PropDensity = 0.45f;
};
