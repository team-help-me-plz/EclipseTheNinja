// Fill out your copyright notice in the Description page of Project Settings.

#include "Checkpoint/CheckpointBoundComponent.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "Interface/CheckpointResettable.h"

#include "Components/PrimitiveComponent.h"

UCheckpointBoundComponent::UCheckpointBoundComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCheckpointBoundComponent::BeginPlay()
{
    Super::BeginPlay();

    if (const AActor* Owner = GetOwner())
    {
        this->InitialTransform = Owner->GetActorTransform();
    }

    if (UCheckpointSubsystem* Subsystem = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
    {
        Subsystem->RegisterBound(this);
    }
}

void UCheckpointBoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 월드 정리 중에는 Subsystem이 이미 없을 수 있음
    if (UWorld* World = GetWorld())
    {
        if (UCheckpointSubsystem* Subsystem = World->GetSubsystem<UCheckpointSubsystem>())
        {
            Subsystem->UnregisterBound(this);
        }
    }

    Super::EndPlay(EndPlayReason);
}

void UCheckpointBoundComponent::ResetOwner()
{
    AActor* Owner = GetOwner();
    if (!IsValid(Owner))
    {
        return;
    }

    if (this->bResetTransform)
    {
        Owner->SetActorTransform(
            this->InitialTransform,
            false,
            nullptr,
            ETeleportType::ResetPhysics
        );

        // 물리 오브젝트가 복원 후 계속 움직이지 않도록 속도 초기화
        if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
        {
            if (Root->IsSimulatingPhysics())
            {
                Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
                Root->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            }
        }
    }

    if (Owner->Implements<UCheckpointResettable>())
    {
        ICheckpointResettable::Execute_ResetToInitial(Owner);
    }
}

void UCheckpointBoundComponent::NotifyAfterReset()
{
    AActor* Owner = GetOwner();
    if (IsValid(Owner) && Owner->Implements<UCheckpointResettable>())
    {
        ICheckpointResettable::Execute_OnAfterReset(Owner);
    }
}
