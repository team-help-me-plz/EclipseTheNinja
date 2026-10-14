// Fill out your copyright notice in the Description page of Project Settings.

#include "Checkpoint/ResettableComponent.h"
#include "Checkpoint/CheckpointSubsystem.h"

#include "Components/PrimitiveComponent.h"

UResettableComponent::UResettableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UResettableComponent::BeginPlay()
{
    Super::BeginPlay();

    // 레벨 시작 상태를 최초 스냅샷으로 저장
    this->CaptureSnapshot();

    if (UCheckpointSubsystem* Subsystem = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
    {
        Subsystem->RegisterResettable(this);
    }
}

void UResettableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (UCheckpointSubsystem* Subsystem = World->GetSubsystem<UCheckpointSubsystem>())
        {
            Subsystem->UnregisterResettable(this);
        }
    }

    Super::EndPlay(EndPlayReason);
}

void UResettableComponent::CaptureSnapshot()
{
    const AActor* Owner = GetOwner();
    if (!IsValid(Owner))
    {
        return;
    }

    this->SnapshotTransform = Owner->GetActorTransform();
    this->bHasSnapshot = true;
}

void UResettableComponent::RestoreSnapshot()
{
    AActor* Owner = GetOwner();
    if (!IsValid(Owner) || !this->bHasSnapshot)
    {
        return;
    }

    Owner->SetActorTransform(
        this->SnapshotTransform,
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
