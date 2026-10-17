// Fill out your copyright notice in the Description page of Project Settings.

#include "ObjectPool/ObjectPoolLevelConfig.h"
#include "ObjectPool/ObjectPoolDataAsset.h"
#include "ObjectPool/ObjectPoolSubsystem.h"

AObjectPoolLevelConfig::AObjectPoolLevelConfig()
{
    PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITORONLY_DATA
    bIsSpatiallyLoaded = false;
#endif
}

void AObjectPoolLevelConfig::BeginPlay()
{
    Super::BeginPlay();

    UObjectPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<UObjectPoolSubsystem>();
    if (!PoolSubsystem)
    {
        return;
    }

    // 레벨 로드 시점이면 모든 액터의 BeginPlay가 끝난 뒤, 이미 플레이 중이면(스트리밍 등) 즉시 생성된다
    for (const FObjectPoolWarmupEntry& Entry : this->WarmupEntries)
    {
        if (Entry.DataAsset)
        {
            PoolSubsystem->RequestWarmup(Entry.DataAsset, Entry.WarmupCount);
        }
    }
}
