// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EclipseTownTypes.generated.h"

class UStaticMesh;

// 집 지붕 계열. 한 블럭 안의 집은 같은 계열만 쓴다
UENUM(BlueprintType)
enum class EEclipseTownRoof : uint8
{
    Tile UMETA(DisplayName = "기와"),
    Wood UMETA(DisplayName = "나무(판자)")
};

// 마을 집 후보 하나
USTRUCT(BlueprintType)
struct FEclipseTownHouseEntry
{
    GENERATED_BODY()

    // 집 프리팹. 폭이 로컬 X, 바닥 Z=0이어야 한다. 피벗 X·Y 위치는 상관없다
    // (노드가 바운드로 X 가운데와 정면을 길 선에 맞춘다. 정면 방향은 bFlipFacing)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town")
    TSoftObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town")
    EEclipseTownRoof Roof = EEclipseTownRoof::Tile;

    // 층수(1 또는 2). TwoStoryRatio로 블럭마다 1층/2층 후보를 고른다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town", meta = (ClampMin = "1", ClampMax = "2"))
    int32 Stories = 1;

    // 같은 계열·층수 후보 중 뽑힐 비중
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town", meta = (ClampMin = "0"))
    float Weight = 1.0f;

    // 0이면 메시 바운드의 X 폭을 쓴다. 바운드에 처마 등이 튀어나와 폭이 맞지 않을 때만 넣는다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town", meta = (ClampMin = "0"))
    float WidthOverride = 0.0f;

    // 켜면 메시 정면이 로컬 +Y인 집으로 보고 180도 돌려 세운다. 로컬 -Y 쪽이 뚫려 있거나 뒷면인 메시(예: SM_JVK_Shop_02)에 켠다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town")
    bool bFlipFacing = false;

    // 집 정면은 메시 바운드 끝(처마·간판 포함)을 길 선에 맞춘다. 처마가 튀어나와 벽이 너무 뒤로 물러나 보이면
    // 그만큼(cm) 넣어 벽 쪽을 길에 붙인다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town", meta = (Units = "cm"))
    float FrontInset = 0.0f;
};

/**
 * 마을 집 세트. PCG_TownRow가 이 목록에서 집을 고른다.
 * 세트를 통째로 바꾸면 같은 배치 규칙으로 다른 테마의 거리가 된다.
 */
UCLASS(BlueprintType)
class ECLIPSETHENINJA_API UEclipseTownHouseSet : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Town")
    TArray<FEclipseTownHouseEntry> Houses;
};
