// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Checkpoint/CheckpointId.h"
#include "CheckpointBoundComponent.generated.h"

/**
 * 액터를 특정 체크포인트 구간에 묶어 CheckpointSubsystem에 등록하는 컴포넌트.
 *  - BeginPlay 시 초기 트랜스폼을 저장하고 Subsystem에 등록, EndPlay 시 해제
 *  - BoundCheckpoint 체크포인트에서 리스폰하면 초기 상태로 리셋된다
 *  - 리셋 시 트랜스폼 복원(옵션) 후 Owner의 ICheckpointResettable 함수를 호출
 *
 * 리셋되어야 하는 액터는 Destroy() 대신 비활성화(숨김/충돌/틱 끄기)할 것. Destroy하면 등록이 해제된다.
 */
UCLASS(ClassGroup = (Checkpoint), meta = (BlueprintSpawnableComponent))
class ECLIPSETHENINJA_API UCheckpointBoundComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCheckpointBoundComponent();

    ECheckpointId GetBoundCheckpoint() const { return this->BoundCheckpoint; }

    // 리스폰 1단계: 트랜스폼 복원 + ICheckpointResettable::ResetToInitial()
    void ResetOwner();

    // 리스폰 2단계: ICheckpointResettable::OnAfterReset()
    void NotifyAfterReset();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    // 이 액터가 속한 구간의 시작 체크포인트.
    // 플레이어가 이 체크포인트에서 리스폰하면 이 액터가 초기 상태로 리셋된다. (다른 체크포인트에서의 리스폰에는 영향 없음)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    ECheckpointId BoundCheckpoint = ECheckpointId::None;

    // 리셋 시 BeginPlay 시점의 트랜스폼으로 되돌릴 것인지 여부 (물리 속도도 초기화)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    bool bResetTransform = true;

private:
    FTransform InitialTransform = FTransform::Identity;

};
