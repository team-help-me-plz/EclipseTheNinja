// Fill out your copyright notice in the Description page of Project Settings.

#include "Checkpoint/CheckpointSubsystem.h"
#include "Checkpoint/Checkpoint.h"
#include "Checkpoint/CheckpointBoundComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

bool UCheckpointSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return (WorldType == EWorldType::Game || WorldType == EWorldType::PIE);
}

void UCheckpointSubsystem::RegisterCheckpoint(ACheckpoint* InCheckpoint)
{
    if (!IsValid(InCheckpoint))
    {
        return;
    }

    const ECheckpointId CheckpointId = InCheckpoint->GetCheckpointId();
    if (CheckpointId == ECheckpointId::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UCheckpointSubsystem::RegisterCheckpoint()] %s 의 CheckpointId가 None입니다."),
               *InCheckpoint->GetName());
        return;
    }

    const TWeakObjectPtr<ACheckpoint>* Existing = this->Checkpoints.Find(CheckpointId);
    if (Existing && Existing->IsValid() && Existing->Get() != InCheckpoint)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UCheckpointSubsystem::RegisterCheckpoint()] %s 가 중복 배치되어 있습니다. (%s, %s)"),
               *UEnum::GetValueAsString(CheckpointId), *Existing->Get()->GetName(), *InCheckpoint->GetName());
    }

    this->Checkpoints.Add(CheckpointId, InCheckpoint);
}

void UCheckpointSubsystem::UnregisterCheckpoint(ACheckpoint* InCheckpoint)
{
    if (!IsValid(InCheckpoint))
    {
        return;
    }

    const ECheckpointId CheckpointId = InCheckpoint->GetCheckpointId();
    if (this->Checkpoints.FindRef(CheckpointId).Get() == InCheckpoint)
    {
        this->Checkpoints.Remove(CheckpointId);
    }
}

void UCheckpointSubsystem::ActivateCheckpoint(ECheckpointId NewCheckpointId)
{
    // 선형 진행: 이미 지나온(같거나 낮은) 체크포인트는 무시
    if (NewCheckpointId == ECheckpointId::None || NewCheckpointId <= this->CurrentCheckpoint)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[UCheckpointSubsystem::ActivateCheckpoint()] 체크포인트 갱신 : %s -> %s"),
           *UEnum::GetValueAsString(this->CurrentCheckpoint), *UEnum::GetValueAsString(NewCheckpointId));

    this->CurrentCheckpoint = NewCheckpointId;
    this->OnCheckpointActivated.Broadcast(NewCheckpointId);
}

void UCheckpointSubsystem::RespawnAtCheckpoint(APawn* PlayerPawn)
{
    const ECheckpointId CheckpointId = this->CurrentCheckpoint;
    if (CheckpointId == ECheckpointId::None)
    {
        return;
    }

    // 순서 주의: 런타임 스폰 액터 파괴와 플레이어 이동을 리셋보다 먼저 한다.
    // 분신/플레이어가 압력 발판에서 벗어나며 발생하는 EndOverlap(문 토글 등)이 리셋 이후에 끼어들지 않도록 하기 위함.

    // 런타임 스폰 액터들 파괴
    this->DestroyRuntimeSpawnedActors();

    // 플레이어를 체크포인트로 이동
    const ACheckpoint* Checkpoint = this->Checkpoints.FindRef(CheckpointId).Get();
    if (!IsValid(Checkpoint))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UCheckpointSubsystem::RespawnAtCheckpoint()] %s 체크포인트를 찾을 수 없어 플레이어를 이동하지 않습니다."),
               *UEnum::GetValueAsString(CheckpointId));
    }
    else if (IsValid(PlayerPawn))
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

    // 리셋 도중 등록/해제가 일어나도 순회가 깨지지 않도록 대상을 복사
    TArray<UCheckpointBoundComponent*> Targets;
    for (const TWeakObjectPtr<UCheckpointBoundComponent>& Weak : this->BoundComponents)
    {
        UCheckpointBoundComponent* Comp = Weak.Get();
        if (IsValid(Comp) && Comp->GetBoundCheckpoint() == CheckpointId)
        {
            Targets.Add(Comp);
        }
    }

    // 1단계: 모든 대상의 데이터 리셋
    for (UCheckpointBoundComponent* Comp : Targets)
    {
        Comp->ResetOwner();
    }

    // 2단계: 모든 대상이 리셋된 뒤 후처리 (다른 액터 상태 참조는 여기서)
    for (UCheckpointBoundComponent* Comp : Targets)
    {
        Comp->NotifyAfterReset();
    }

    UE_LOG(LogTemp, Log, TEXT("[UCheckpointSubsystem::RespawnAtCheckpoint()] 플레이어가 체크포인트 %s 에서 리스폰 (리셋된 액터 %d개)"),
           *UEnum::GetValueAsString(CheckpointId), Targets.Num());

    this->OnRespawned.Broadcast(CheckpointId);
}

void UCheckpointSubsystem::RegisterBound(UCheckpointBoundComponent* InComponent)
{
    if (!IsValid(InComponent))
    {
        return;
    }

    /*
    * 디버그 목적으로 "모든 컴포넌트를 나열"할 필요가 있어지거나,
    * "체크포인트가 None이면 무조건 리셋" 같은 기능이 필요해지면 this->BoundComponents 에 등록
    if (InComponent->GetBoundCheckpoint() == ECheckpointId::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UCheckpointSubsystem::RegisterBound()] %s 는 속한 체크포인트가 None입니다."),
               *GetNameSafe(InComponent->GetOwner()));
    }
    */

    if (InComponent->GetBoundCheckpoint() == ECheckpointId::None)
    {
        // 퍼즐 클래스들이 컴포넌트를 기본으로 가지므로 체크포인트를 쓰지 않는 레벨에서는 None이 정상. Warning 대신 Log
        UE_LOG(LogTemp, Log, TEXT("[UCheckpointSubsystem::RegisterBound()] %s 는 속한 체크포인트가 None이라 등록되지 않습니다."),
               *GetNameSafe(InComponent->GetOwner()));
        return;
    }

    this->BoundComponents.Add(InComponent);
}

void UCheckpointSubsystem::UnregisterBound(UCheckpointBoundComponent* InComponent)
{
    this->BoundComponents.Remove(InComponent);
}

void UCheckpointSubsystem::RegisterRuntimeSpawnedActor(AActor* InActor)
{
    if (!IsValid(InActor))
    {
        return;
    }

    // 이미 파괴된 항목 정리
    this->RuntimeSpawnedActors.RemoveAll(
        [](const TWeakObjectPtr<AActor>& Weak) {
            return !Weak.IsValid();
        });

    this->RuntimeSpawnedActors.Add(InActor);
}

void UCheckpointSubsystem::DestroyRuntimeSpawnedActors()
{
    // 파괴 도중 등록/해제가 일어나도 순회가 깨지지 않도록 목록을 옮긴 뒤 순회
    TArray<TWeakObjectPtr<AActor>> Actors = MoveTemp(this->RuntimeSpawnedActors);
    this->RuntimeSpawnedActors.Reset();

    for (const TWeakObjectPtr<AActor>& Weak : Actors)
    {
        if (AActor* Actor = Weak.Get())
        {
            Actor->Destroy();
        }
    }
}
