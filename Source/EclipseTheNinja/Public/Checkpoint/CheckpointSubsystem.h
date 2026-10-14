// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CheckpointSubsystem.generated.h"

class ACheckpoint;
class UResettableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCheckpointEvent, ACheckpoint*, Checkpoint);

/**
 * 체크포인트 서브시스템.
 *  - 월드의 모든 UResettableComponent가 BeginPlay 시 등록된다.
 *  - 커밋 : 체크포인트 활성화 시 등록되어 있던 컴포넌트들의 현재 상태를 스냅샷으로 저장
 *  - 롤백 : 리스폰 시 모든 컴포넌트를 스냅샷으로 되돌리고 플레이어를 체크포인트로 이동
 */
UCLASS()
class ECLIPSETHENINJA_API UCheckpointSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // 체크포인트를 활성화하고 현재 컴포넌트들의 상태를 커밋하는 함수
    UFUNCTION(BlueprintCallable, Category = "Checkpoint")
    void ActivateCheckpoint(ACheckpoint* NewCheckpoint);

    // 모든 컴포넌트들을 롤백한 뒤, 플레이어를 체크포인트로 이동시키는 함수
    UFUNCTION(BlueprintCallable, Category = "Checkpoint")
    void RespawnAtCheckpoint(APawn* PlayerPawn);

    UFUNCTION(BlueprintPure, Category = "Checkpoint")
    ACheckpoint* GetCurrentCheckpoint() const { return this->CurrentCheckpoint.Get(); }

    void RegisterResettable(UResettableComponent* InComponent);
    void UnregisterResettable(UResettableComponent* InComponent);

protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

public:
    // 체크포인트가 갱신될 때 실행되는 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Checkpoint")
    FOnCheckpointEvent OnCheckpointActivated;

    // 리스폰될 때 실행되는 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Checkpoint")
    FOnCheckpointEvent OnRespawned;

private:
    TWeakObjectPtr<ACheckpoint> CurrentCheckpoint;
    TSet<TWeakObjectPtr<UResettableComponent>> Resettables;

};
