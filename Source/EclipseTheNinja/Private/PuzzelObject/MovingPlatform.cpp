// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzelObject/MovingPlatform.h"
#include "Checkpoint/CheckpointBoundComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AMovingPlatform::AMovingPlatform()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    this->RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    SetRootComponent(this->RootScene);

    this->MeshAssembly = CreateDefaultSubobject<UChildActorComponent>(TEXT("MeshAssembly"));
    this->MeshAssembly->SetMobility(EComponentMobility::Movable);
    this->MeshAssembly->SetupAttachment(GetRootComponent());

    this->CheckpointBound = CreateDefaultSubobject<UCheckpointBoundComponent>(TEXT("CheckpointBound"));
}

void AMovingPlatform::BeginPlay()
{
    Super::BeginPlay();

    //this->ElapsedTime = 0.0;
    this->EndPointFloat = static_cast<float>(this->TargetPoint.Num()) - 1.0f;
    this->CycleLength = EndPointFloat * 2.0f;
    this->bIsMovementUse = this->bIsMovement;
}

void AMovingPlatform::PlatformMovement(float DeltaTime)
{
    if (this->TargetPoint.Num() < 2)
    {
        return;
    }
    if (!this->bIsMovementUse && !this->bIsResetable)
    {
        return;
    }
    if (this->TargetPointTravelTime == 0.f)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[AMovingPlatform::PlatformMovement] %s의 TargetPointTravelTime이 0입니다"),
            *this->GetName()
        );
        return;
    }
    const float	Step = DeltaTime / this->TargetPointTravelTime;
    if (this->bIsMovementUse)
    {
        float	Phase = this->bIsMoveForward
            ? this->RouteProgress
            : this->CycleLength - this->RouteProgress;
        Phase = FMath::Fmod(Phase + Step, CycleLength);
        this->bIsMoveForward = Phase < this->EndPointFloat;
        this->RouteProgress = this->bIsMoveForward
            ? Phase
            : this->CycleLength - Phase;
    }
    else if (this->bIsResetable)
    {
        this->RouteProgress = FMath::Max(this->RouteProgress - Step, 0.0);
        this->bIsMoveForward = true;
    }
    const int32		SegmentIndex = FMath::Min(FMath::FloorToInt(this->RouteProgress), this->TargetPoint.Num() - 2);
    const double	Alpha = this->RouteProgress - SegmentIndex;

    SetActorLocation(FMath::Lerp(
        this->TargetPoint[SegmentIndex]->GetActorLocation(),
        this->TargetPoint[SegmentIndex + 1]->GetActorLocation(),
        Alpha));
}

void AMovingPlatform::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    this->PlatformMovement(DeltaTime);
}

void AMovingPlatform::ToggleActivate_Implementation()
{
    if (!this->bIsActivatable)
    {
        return;
    }
    this->bIsMovementUse = !this->bIsMovementUse;
}

void AMovingPlatform::ResetToInitial_Implementation()
{
    this->bIsMovementUse = this->bIsMovement;
    this->RouteProgress = 0.f;
    if (this->TargetPoint.Num())
    {
        this->SetActorLocation(this->TargetPoint[0]->GetActorLocation());
    }
}

void AMovingPlatform::OnAfterReset_Implementation()
{
}


