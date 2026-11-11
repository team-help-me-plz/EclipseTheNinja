// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Checkpoint/CheckpointId.h"
#include "Checkpoint.generated.h"

class UBoxComponent;
class UArrowComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;
class USoundBase;

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

    // 레벨 시작 체크포인트인지. CheckpointId가 None이라 등록되지 않은 레벨에서 대체 복귀 위치를 찾을 때 쓴다
    bool IsActivateOnBeginPlay() const { return this->bActivateOnBeginPlay; }

    UFUNCTION(BlueprintPure, Category = "Checkpoint")
    bool IsCheckpointActive() const { return bCheckpointActive; }

    // 서브시스템의 현재 체크포인트에 맞춰 가로등 표시를 갱신한다.
    void SetCheckpointActive(bool bActive);

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> LampMesh = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UPointLightComponent> LampLight = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Checkpoint|Lamp")
    TObjectPtr<UMaterialInterface> LampOffMaterial = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Checkpoint|Lamp")
    TObjectPtr<UMaterialInterface> LampOnMaterial = nullptr;

    // 플레이어가 닿아 이 체크포인트로 갱신될 때 재생할 사운드. 레벨 시작 자동 활성화와 이어하기 복원 때는 재생하지 않는다
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Checkpoint|Sound")
    TObjectPtr<USoundBase> ActivateSound = nullptr;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Checkpoint")
    bool bCheckpointActive = false;

    // 이 체크포인트의 식별자. 순서와 관계없이 마지막으로 닿은 체크포인트가 재시작 위치가 된다. None이면 등록되지 않는다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    ECheckpointId CheckpointId = ECheckpointId::None;

    // 레벨 시작 체크포인트용 변수. BeginPlay에서 자동 활성화
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    bool bActivateOnBeginPlay = false;

private:
    void ApplyLampState();
};
