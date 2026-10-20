// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ObjectPool/ObjectPoolTypes.h"
#include "ObjectPoolDataAsset.generated.h"

/**
 * 오브젝트 풀하나에 대한 설정 데이터 에셋.
 * 에셋을 만든 뒤 Project Settings > Game > Object Pool 의 목록에 추가하면 모든 월드에서 풀이 등록된다.
 * 레벨별로 미리 생성할 풀과 수량은 레벨에 배치한 AObjectPoolLevelConfig에서 지정한다.
 * (런타임에 UObjectPoolSubsystem::RegisterPool로 직접 등록하는 것도 가능)
 */
UCLASS(BlueprintType)
class ECLIPSETHENINJA_API UObjectPoolDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // 풀링할 액터 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ObjectPool")
    TSoftClassPtr<AActor> ActorClass = nullptr;

    // 미리 생성할 기본 수량 (레벨 설정에서 수량을 따로 지정하지 않았을 때 사용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ObjectPool", meta = (ClampMin = "0"))
    int32 InitialSize = 0;

    // true면 모든 레벨에서 InitialSize만큼 미리 생성한다 (분신처럼 어느 레벨에서나 쓰이는 액터용, 프로젝트 세팅에 등록된 경우만 적용)
    // false면 레벨에 배치된 AObjectPoolLevelConfig에 등록된 레벨에서만 미리 생성하고, 그 외에는 처음 스폰될 때 생성한다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ObjectPool")
    bool bWarmupInAllLevels = false;

    // 풀이 관리하는 최대 수량 (대기 + 사용 중). 0이면 무제한
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ObjectPool", meta = (ClampMin = "0"))
    int32 MaxSize = 0;

    // 최대 수량에 도달했을 때의 처리 방식 (MaxSize가 0이면 사용되지 않음)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ObjectPool", meta = (EditCondition = "MaxSize > 0"))
    EObjectPoolOverflowPolicy OverflowPolicy = EObjectPoolOverflowPolicy::Grow;

};
