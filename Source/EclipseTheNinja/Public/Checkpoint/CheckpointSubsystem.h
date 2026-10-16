// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Checkpoint/CheckpointId.h"
#include "CheckpointSubsystem.generated.h"

class ACheckpoint;
class UCheckpointBoundComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCheckpointEvent, ECheckpointId, CheckpointId);

/**
 * 체크포인트 서브시스템.
 *  - 갱신   : 현재보다 이후의 체크포인트에 도달하면 CurrentCheckpoint를 갱신한다.
 *  - 리스폰 : BoundCheckpoint == CurrentCheckpoint 인 UCheckpointBoundComponent들의 Owner를 초기 상태로 리셋하고,
 *             런타임 스폰 액터를 파괴한 뒤 플레이어를 체크포인트로 이동시킨다.
 *  - 리셋   : 모든 대상 ResetToInitial -> 모든 대상 OnAfterReset
 */
UCLASS()
class ECLIPSETHENINJA_API UCheckpointSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // 체크포인트를 서브시스템에 등록하는 함수
    // 리스폰 시 체크포인트의 리스폰 위치를 가져오기 위함
    void RegisterCheckpoint(ACheckpoint* InCheckpoint);

    // 체크포인트를 서브시스템에서 제거하는 함수
    void UnregisterCheckpoint(ACheckpoint* InCheckpoint);

    // 현재보다 이후의(값이 큰) 체크포인트일 때만 갱신
    UFUNCTION(BlueprintCallable, Category = "Checkpoint")
    void ActivateCheckpoint(ECheckpointId NewCheckpointId);

    // 현재 체크포인트 구간의 오브젝트들을 리셋한 뒤, 플레이어를 체크포인트로 이동시키는 함수
    UFUNCTION(BlueprintCallable, Category = "Checkpoint")
    void RespawnAtCheckpoint(APawn* PlayerPawn);

    UFUNCTION(BlueprintPure, Category = "Checkpoint")
    ECheckpointId GetCurrentCheckpoint() const { return this->CurrentCheckpoint; }

    UFUNCTION(BlueprintCallable, Category = "Checkpoint")
    bool HasValidCheckpoint() const { return this->CurrentCheckpoint != ECheckpointId::None; }

    // Bound 컴포넌트를 서브시스템에 등록하는 함수
    void RegisterBound(UCheckpointBoundComponent* InComponent);

    // Bound 컴포넌트를 서브시스템에서 해제하는 함수
    void UnregisterBound(UCheckpointBoundComponent* InComponent);

    // 리스폰 시 파괴될 런타임 스폰 액터(발사체, 증원 몬스터 등)를 등록하는 함수
    UFUNCTION(BlueprintCallable, Category = "Checkpoint")
    void RegisterRuntimeSpawnedActor(AActor* InActor);

protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

public:
    // 체크포인트가 갱신될 때 실행되는 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Checkpoint")
    FOnCheckpointEvent OnCheckpointActivated;

    // 리셋과 플레이어 이동이 끝난 뒤 실행되는 델리게이트 (UI, 카메라 등이 사용)
    UPROPERTY(BlueprintAssignable, Category = "Checkpoint")
    FOnCheckpointEvent OnRespawned;

private:
    // 런타임 스폰 액터(발사체, 증원 몬스터 등)들을 파괴하는 함수
    // TODO: 오브젝트 풀을 도입할 경우 풀에 반환하는 방식으로 수정
    void DestroyRuntimeSpawnedActors();

private:
    ECheckpointId CurrentCheckpoint = ECheckpointId::None;

    // 등록된 체크포인트 Map
    TMap<ECheckpointId, TWeakObjectPtr<ACheckpoint>> Checkpoints;

    // Bound 컴포넌트(UCheckpointBoundComponent)들
    TSet<TWeakObjectPtr<UCheckpointBoundComponent>> BoundComponents;

    // 런타임 스폰 액터(발사체, 증원 몬스터 등)들
    TArray<TWeakObjectPtr<AActor>> RuntimeSpawnedActors;

};
