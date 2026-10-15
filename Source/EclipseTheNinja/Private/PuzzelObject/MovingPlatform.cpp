// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzelObject/MovingPlatform.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AMovingPlatform::AMovingPlatform()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	this->Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	this->Mesh->SetMobility(EComponentMobility::Movable);
	SetRootComponent(this->Mesh);
}

void AMovingPlatform::BeginPlay()
{
	Super::BeginPlay();

	//this->ElapsedTime = 0.0;
	this->EndPointFloat = static_cast<float>(this->TargetPoint.Num());
	this->CycleLength = EndPointFloat * 2.0f;
}

void AMovingPlatform::PlatformMovement(float DeltaTime)
{
	if (this->TargetPoint.Num() < 2)
	{
		return ;
	}
	if (!this->bIsMovement && !this->bIsResetable)
	{
		return ;
	}
	if (this->TargetPointTravelTime == 0.f)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[AMovingPlatform::PlatformMovement] %s의 TargetPointTravelTime이 0입니다"),
			*this->GetName()
		);
		return ;
	}
	const float	Step = DeltaTime / this->TargetPointTravelTime;
	if (this->bIsMovement)
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
	this->bIsMovement = !this->bIsMovement;
}


