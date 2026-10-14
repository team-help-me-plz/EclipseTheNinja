// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Checkpoint.generated.h"

class UBoxComponent;
class UArrowComponent;

/**
 * 체크포인트 액터.
 *  - ActivationTrigger : 플레이어가 닿으면 활성화(커밋)
 *  - RespawnPoint      : 리스폰 위치/방향
 */
UCLASS()
class ECLIPSETHENINJA_API ACheckpoint : public AActor
{
    GENERATED_BODY()

public:
    ACheckpoint();

    int32 GetCheckpointOrder() const { return this->CheckpointOrder; }
    FTransform GetRespawnTransform() const;

protected:
    virtual void BeginPlay() override;

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

    // 체크포인트 순서. 이보다 낮거나 같은 체크포인트로 되돌아가 활성화(재커밋)되는 것을 막는다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    int32 CheckpointOrder = 0;

    // 레벨 시작 체크포인트용 변수. BeginPlay에서 자동 활성화
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    bool bActivateOnBeginPlay = false;

};
