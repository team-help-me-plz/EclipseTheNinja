// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ObjectPoolSettings.generated.h"

class UObjectPoolDataAsset;

/**
 * Project Settings > Game > Object Pool
 * Config/DefaultGame.ini에 저장되는 설정값으로,
 * 여기에 등록된 데이터 에셋들은 모든 게임 월드에서 풀이 등록된다.
 * 미리 생성은 데이터 에셋의 bWarmupInAllLevels 또는 레벨의 AObjectPoolLevelConfig 설정을 따른다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Object Pool"))
class ECLIPSETHENINJA_API UObjectPoolSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual FName GetCategoryName() const override { return TEXT("Game"); }

public:
    UPROPERTY(Config, EditAnywhere, Category = "ObjectPool")
    TArray<TSoftObjectPtr<UObjectPoolDataAsset>> PoolDataAssets;

};
