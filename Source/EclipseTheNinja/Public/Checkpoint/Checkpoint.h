// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Checkpoint/CheckpointId.h"
#include "Checkpoint.generated.h"

class APawn;
class UBoxComponent;
class UArrowComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USoundBase;

/**
 * 체크포인트 액터.
 *  - ActivationTrigger : 플레이어가 닿으면 CheckpointId로 갱신 요청
 *  - RespawnPoint      : 리스폰 위치/방향
 *  - 기본적으로 BeginPlay 시 CheckpointSubsystem에 등록. 같은 CheckpointId는 레벨에 하나만 배치할 것
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
    void RegisterAfterSequence();

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

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

    // 에디터에서 보이는 가로등 머티리얼
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Checkpoint|Lamp")
    TObjectPtr<UMaterialInterface> LampOffMaterial = nullptr;

    // 게임 중 가로등 머티리얼. 이 머티리얼로 동적 인스턴스를 만들어 발광 세기를 연출에 맞춰 바꾼다 (평소 0)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Checkpoint|Lamp")
    TObjectPtr<UMaterialInterface> LampOnMaterial = nullptr;

    // 가로등 머티리얼의 발광 세기 스칼라 파라미터 이름
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Checkpoint|Lamp")
    FName EmissiveParameterName = TEXT("Emissive Strength");

    // 플레이어가 닿을 때마다 재생할 사운드(현재 체크포인트에 다시 닿을 때 포함). 밝기 연출 한 사이클 동안의 중복 접촉, 레벨 시작 자동 활성화, 이어하기 복원 때는 재생하지 않는다
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Checkpoint|Sound")
    TObjectPtr<USoundBase> ActivateSound = nullptr;

    // 플레이어가 닿아 갱신될 때 가로등 빛이 0에서 밝아졌다 다시 0으로 어두워지는 데 걸리는 시간(초). 0이면 연출하지 않는다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Lamp", meta = (ClampMin = "0.0", Units = "s"))
    float FlashDuration = 0.8f;

    // 연출 중 가로등 빛의 최대 밝기(루멘). 평소 밝기는 0이다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Lamp", meta = (ClampMin = "0.0", Units = "lm"))
    float FlashPeakIntensity = 4000.0f;

    // 연출 중 가로등 메시의 최대 발광 세기(EmissiveParameterName 값). 평소 발광 세기는 0이다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint|Lamp", meta = (ClampMin = "0.0"))
    float FlashPeakEmissive = 20.0f;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Checkpoint")
    bool bCheckpointActive = false;

    // 이 체크포인트의 식별자. 순서와 관계없이 마지막으로 닿은 체크포인트가 재시작 위치가 된다. None이면 등록되지 않는다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    ECheckpointId CheckpointId = ECheckpointId::None;

    // 레벨 시작 체크포인트용 변수. BeginPlay에서 자동 활성화
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    bool bActivateOnBeginPlay = false;

    // 인트로가 끝나기 전에는 등록과 오버랩 활성화를 보류한다.
    UPROPERTY(EditInstanceOnly, Category = "Checkpoint")
    bool bWaitForSequence = false;

private:
    bool bRegisteredAfterSequence = false;
    void TryActivateForPawn(const APawn* Pawn);
    void ApplyLampState();

    // 가로등 밝기 연출을 시작/중단한다. 중단하면 평소 밝기로 되돌린다
    void StartLampFlash();
    void StopLampFlash();

    // 빛 밝기와 메시 발광 세기를 함께 맞춘다. Weight는 0(평소) ~ 1(최대)
    void SetLampFlashWeight(float Weight);

    // 게임 중 가로등 메시의 동적 머티리얼 인스턴스
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> LampMID = nullptr;

    float FlashElapsed = 0.0f;
    bool bFlashing = false;
};
