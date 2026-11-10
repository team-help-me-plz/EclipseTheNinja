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
 *  - 갱신   : 현재와 다른 체크포인트에 도달하면 ID 크기와 관계없이 CurrentCheckpoint를 갱신한다.
 *  - 리스폰 : BoundCheckpoint == CurrentCheckpoint 이거나 bAlwaysResetOnRespawn이 켜진 UCheckpointBoundComponent들의 Owner를 초기 상태로 리셋하고,
 *             런타임 스폰 액터를 풀에 반환(풀 액터가 아니면 파괴)한 뒤 플레이어를 체크포인트로 이동시킨다.
 *  - 리셋   : 모든 대상 ResetToInitial -> 모든 대상 OnAfterReset
 */
UCLASS()
class ECLIPSETHENINJA_API UCheckpointSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    void RestoreCheckpoint(ECheckpointId Id);
    bool GetCheckpointTransform(FTransform& OutTransform) const;

    // 현재 체크포인트를 바꾸지 않고, 이 맵에 배치된 Id 체크포인트의 리스폰 위치만 읽는다. 없으면 false
    bool GetCheckpointTransformById(ECheckpointId Id, FTransform& OutTransform) const;

    // 세이프 포인트가 없을 때 돌아갈 위치를 찾는다. 현재 체크포인트가 있으면 그 위치,
    // 없으면 bActivateOnBeginPlay가 켜진 체크포인트 중 FromLocation에 가장 가까운 것의 위치. 둘 다 없으면 false
    bool GetFallbackRespawnTransform(const FVector& FromLocation, FTransform& OutTransform) const;

    // 체크포인트를 서브시스템에 등록하는 함수
    // 리스폰 시 체크포인트의 리스폰 위치를 가져오기 위함
    void RegisterCheckpoint(ACheckpoint* InCheckpoint);

    // 체크포인트를 서브시스템에서 제거하는 함수
    void UnregisterCheckpoint(ACheckpoint* InCheckpoint);

    // ID 크기와 관계없이 마지막으로 닿은 체크포인트로 갱신한다. 현재 체크포인트와 같으면 무시
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

    // 리스폰 시 풀에 반환(또는 파괴)될 런타임 스폰 액터(발사체, 증원 몬스터 등)를 등록하는 함수
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
    void UpdateCheckpointLights();

    // 런타임 스폰 액터(발사체, 증원 몬스터 등)들을 풀에 반환하는 함수 (풀 액터가 아니면 파괴)
    void ReleaseRuntimeSpawnedActors();

private:
    ECheckpointId CurrentCheckpoint = ECheckpointId::None;

    // 등록된 체크포인트 Map
    TMap<ECheckpointId, TWeakObjectPtr<ACheckpoint>> Checkpoints;

    // Bound 컴포넌트(UCheckpointBoundComponent)들
    TSet<TWeakObjectPtr<UCheckpointBoundComponent>> BoundComponents;

    // 런타임 스폰 액터(발사체, 증원 몬스터 등)들
    TArray<TWeakObjectPtr<AActor>> RuntimeSpawnedActors;

};
