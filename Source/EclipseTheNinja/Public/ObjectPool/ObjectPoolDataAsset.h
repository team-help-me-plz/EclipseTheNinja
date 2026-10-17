// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ObjectPool/ObjectPoolTypes.h"
#include "ObjectPoolDataAsset.generated.h"

/**
 * 오브젝트 풀하나에 대한 설정 데이터 에셋.
 * 에셋을 만든 뒤 Project Settings > Game > Object Pool 의 목록에 추가하면 월드 시작 시 자동으로 풀이 만들어진다.
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

    // 월드 시작 시 미리 생성해 둘 수량
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ObjectPool", meta = (ClampMin = "0"))
    int32 InitialSize = 0;

    // 풀이 관리하는 최대 수량 (대기 + 사용 중). 0이면 무제한
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ObjectPool", meta = (ClampMin = "0"))
    int32 MaxSize = 0;

    // 최대 수량에 도달했을 때의 처리 방식 (MaxSize가 0이면 사용되지 않음)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ObjectPool", meta = (EditCondition = "MaxSize > 0"))
    EObjectPoolOverflowPolicy OverflowPolicy = EObjectPoolOverflowPolicy::Grow;

};
