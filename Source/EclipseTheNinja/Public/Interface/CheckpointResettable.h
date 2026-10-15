// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CheckpointResettable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UCheckpointResettable : public UInterface
{
    GENERATED_BODY()
};

/**
 * 체크포인트 리스폰 시 초기 상태로 돌아가야 하는 액터가 상속 및 구현한다.
 * UCheckpointBoundComponent가 붙어 있어야만 호출된다.
 * 기본적으로 UCheckpointBoundComponent가 트랜스폼을 복원하기 때문에,
 * 위치만 되돌리면 되는 액터(밀 수 있는 상자 등)는 구현하지 않아도 된다.
 */
class ECLIPSETHENINJA_API ICheckpointResettable
{
    GENERATED_BODY()

public:
    // 리스폰 1단계. 자기 자신의 데이터만 초기 상태로 되돌린다. (다른 액터의 상태를 읽지 말 것)
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Checkpoint")
    void ResetToInitial();

    // 리스폰 2단계. 모든 대상의 ResetToInitial이 끝난 뒤 호출된다.
    // 비주얼 갱신, AI 재시작, 다른 액터 상태 반영(스위치 -> 문) 등을 처리한다.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Checkpoint")
    void OnAfterReset();
};
