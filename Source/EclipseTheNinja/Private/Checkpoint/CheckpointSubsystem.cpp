// Fill out your copyright notice in the Description page of Project Settings.

#include "Checkpoint/CheckpointSubsystem.h"
#include "Checkpoint/Checkpoint.h"
#include "Checkpoint/ResettableComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

bool UCheckpointSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return (WorldType == EWorldType::Game || WorldType == EWorldType::PIE);
}

void UCheckpointSubsystem::ActivateCheckpoint(ACheckpoint* NewCheckpoint)
{
    if (!IsValid(NewCheckpoint) || NewCheckpoint == this->CurrentCheckpoint.Get())
    {
        return;
    }

    // 아직 활성화된 체크포인트가 없으면(레벨 시작) 순서 비교 없이 통과
    if (this->CurrentCheckpoint.IsValid()
        && NewCheckpoint->GetCheckpointOrder() <= this->CurrentCheckpoint->GetCheckpointOrder())
    {
        return;
    }

    // 현재 상태를 새 기준으로 확정
    for (const TWeakObjectPtr<UResettableComponent>& Weak : this->Resettables)
    {
        if (UResettableComponent* Comp = Weak.Get())
        {
            Comp->CaptureSnapshot();
        }
    }

    this->CurrentCheckpoint = NewCheckpoint;
    this->OnCheckpointActivated.Broadcast(NewCheckpoint);
}

void UCheckpointSubsystem::RespawnAtCheckpoint(APawn* PlayerPawn)
{
    ACheckpoint* Checkpoint = this->CurrentCheckpoint.Get();
    if (!IsValid(Checkpoint))
    {
        return;
    }

    // 상태 되돌리기
    for (const TWeakObjectPtr<UResettableComponent>& Weak : this->Resettables)
    {
        if (UResettableComponent* Comp = Weak.Get())
        {
            Comp->RestoreSnapshot();
        }
    }

    if (IsValid(PlayerPawn))
    {
        const FTransform RespawnTransform = Checkpoint->GetRespawnTransform();
        PlayerPawn->SetActorLocationAndRotation(
            RespawnTransform.GetLocation(),
            RespawnTransform.GetRotation(),
            false,
            nullptr,
            ETeleportType::ResetPhysics
        );

        if (AController* Controller = PlayerPawn->GetController())
        {
            Controller->SetControlRotation(RespawnTransform.Rotator());
        }
    }

    this->OnRespawned.Broadcast(Checkpoint);
}

void UCheckpointSubsystem::RegisterResettable(UResettableComponent* InComponent)
{
    if (IsValid(InComponent))
    {
        this->Resettables.Add(InComponent);
    }
}

void UCheckpointSubsystem::UnregisterResettable(UResettableComponent* InComponent)
{
    this->Resettables.Remove(InComponent);
}
