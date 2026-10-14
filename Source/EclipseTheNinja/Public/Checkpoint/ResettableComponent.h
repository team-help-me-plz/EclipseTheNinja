// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ResettableComponent.generated.h"

/**
 * 체크포인트 리스폰 시 초기화될 액터에 부착하는 컴포넌트.
 *  - BeginPlay 시 초기 상태를 스냅샷으로 저장하고 Subsystem에 등록
 *  - 체크포인트 활성화(커밋) 시 현재 상태를 스냅샷으로 갱신
 *  - 리스폰(롤백) 시 스냅샷으로 되돌림 (트랜스폼 + 물리 속도 초기화)
 */
UCLASS(ClassGroup = (Checkpoint), meta = (BlueprintSpawnableComponent))
class ECLIPSETHENINJA_API UResettableComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UResettableComponent();

    void CaptureSnapshot();
    void RestoreSnapshot();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FTransform SnapshotTransform = FTransform::Identity;
    bool bHasSnapshot = false;

};
