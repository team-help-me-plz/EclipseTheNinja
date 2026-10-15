// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Checkpoint/CheckpointId.h"
#include "Checkpoint.generated.h"

class UBoxComponent;
class UArrowComponent;

/**
 * 체크포인트 액터.
 *  - ActivationTrigger : 플레이어가 닿으면 CheckpointId로 갱신 요청
 *  - RespawnPoint      : 리스폰 위치/방향
 *  - BeginPlay 시 CheckpointSubsystem에 등록 (리스폰 위치 조회용). 같은 CheckpointId는 레벨에 하나만 배치할 것
 */
UCLASS()
class ECLIPSETHENINJA_API ACheckpoint : public AActor
{
    GENERATED_BODY()

public:
    ACheckpoint();

    ECheckpointId GetCheckpointId() const { return this->CheckpointId; }
    FTransform GetRespawnTransform() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void OnTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> ActivationTrigger = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UArrowComponent> RespawnPoint = nullptr;

    // 이 체크포인트의 식별자. Enum 순서가 진행 순서이므로, 이미 지나온 체크포인트는 다시 활성화되지 않는다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    ECheckpointId CheckpointId = ECheckpointId::None;

    // 레벨 시작 체크포인트용 변수. BeginPlay에서 자동 활성화
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    bool bActivateOnBeginPlay = false;

};
