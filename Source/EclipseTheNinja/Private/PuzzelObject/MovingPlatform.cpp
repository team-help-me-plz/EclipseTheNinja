// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzelObject/MovingPlatform.h"
#include "Checkpoint/CheckpointBoundComponent.h"
#include "Component/ActivateHighlightComponent.h"
#include "Save/GameSaveSubsystem.h"

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
    this->ActivateHighlight = CreateDefaultSubobject<UActivateHighlightComponent>(TEXT("ActivateHighlight"));
}

void AMovingPlatform::BeginPlay()
{
    Super::BeginPlay();

    //this->ElapsedTime = 0.0;
    this->EndPointFloat = static_cast<float>(this->TargetPoint.Num()) - 1.0f;
    this->CycleLength = EndPointFloat * 2.0f;
    this->bIsMovementUse = this->bIsMovement;

    // 이어하기라면 저장된 켜짐 상태와 위치에서 시작한다. DeltaTime 0으로 위치만 맞춘다
    UGameSaveSubsystem* Save = UGameSaveSubsystem::Get(this);
    FSavedActorState State;

    if (Save && Save->TakeActorState(this, State))
    {
        this->bIsMovementUse = State.bIsOn;
        this->RouteProgress = State.Progress;
        this->PlatformMovement(0.f);
    }
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

    // 원위치로 돌아가지 않는 발판은 꺼진 순간의 위치에 멈추므로 진행도도 함께 저장한다
    if (UGameSaveSubsystem* Save = UGameSaveSubsystem::Get(this))
    {
        FSavedActorState State;
        State.bIsOn = this->bIsMovementUse;
        State.Progress = this->RouteProgress;
        Save->RecordActorState(this, State);
    }
}

void AMovingPlatform::ShowActivationIndicator_Implementation()
{
    this->ActivateHighlight->ShowHighlight();
}

void AMovingPlatform::HideActivationIndicator_Implementation()
{
    this->ActivateHighlight->HideHighlight();
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


