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

	this->ElapsedTime = 0.0;
}

void AMovingPlatform::PlatformMovement(float DeltaTime)
{
	if (this->TargetPoint.Num() < 2)
	{
		return ;
	}
	if (this->bIsMovement)
	{
		this->ElapsedTime += DeltaTime;
		if (GetActorLocation().Equals(this->TargetPoint[this->NextTargetPoint]->GetActorLocation()))
		{
			if (this->bIsMoveForward)
			{
				this->CurrentTargetPoint++;
			}
			else
			{
				this->CurrentTargetPoint--;
			}
			if (this->NextTargetPoint == 0 || this->NextTargetPoint == this->TargetPoint.Num() - 1)
			{
				this->bIsMoveForward = !this->bIsMoveForward;
			}
			if (this->bIsMoveForward)
			{
				this->NextTargetPoint++;
			}
			else
			{
				this->NextTargetPoint--;
			}
			this->ElapsedTime = 0.0;
		}
		const double	Alpha = FMath::Clamp(this->ElapsedTime / this->TargetPointTravelTime, 0.0, 1.0);
		const FVector	NewLocation = FMath::Lerp(
			this->TargetPoint[this->CurrentTargetPoint]->GetActorLocation(),
			this->TargetPoint[this->NextTargetPoint]->GetActorLocation(),
			Alpha
		);
		SetActorLocation(NewLocation);
	}
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


