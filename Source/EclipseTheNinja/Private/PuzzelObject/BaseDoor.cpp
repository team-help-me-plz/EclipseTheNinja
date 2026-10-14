// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzelObject/BaseDoor.h"
#include "Interface/Interactor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ABaseDoor::ABaseDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	this->RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(this->RootComponent);

	this->RightDoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightDoorMesh"));
	this->RightDoorMesh->SetMobility(EComponentMobility::Movable);
	this->RightDoorMesh->SetupAttachment(GetRootComponent());

	this->LeftDoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftDoorMesh"));
	this->LeftDoorMesh->SetMobility(EComponentMobility::Movable);
	this->LeftDoorMesh->SetupAttachment(GetRootComponent());
}

void ABaseDoor::BeginPlay()
{
	Super::BeginPlay();

	//this->CloseLocation = GetActorLocation();
	//this->OpenLocation = GetActorLocation() + this->OpenOffset;
	this->OpenAlpha = 0.0;
}

void ABaseDoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	this->DoorMovement(DeltaTime);
}

void ABaseDoor::ToggleActivate_Implementation()
{
	this->bIsOpen = !this->bIsOpen;
}

void ABaseDoor::Interact_Implementation(AActor* InActor)
{
	if (!this->bIsInteractable || this->bIsInteractCooldown)
	{
		return ;
	}
	if (!IsValid(InActor) || !InActor->Implements<UInteractor>())
	{
		return ;
	}
	this->bIsInteractCooldown = true;
	GetWorldTimerManager().SetTimer(
		InteractionCooldownTimer,
		this,
		&ABaseDoor::ResetInteractionCooldown,
		this->InteractInterval,
		false
	);
	this->bIsOpen = !this->bIsOpen;
}

void ABaseDoor::ShowInteractionIndicator_Implementation()
{
	if (!this->bIsInteractable)
	{
		return;
	}
}

void ABaseDoor::HideInteractionIndicator_Implementation()
{
	if (!this->bIsInteractable)
	{
		return;
	}
}

bool ABaseDoor::bIsInteractReady_Implementation()
{
	return (this->bIsInteractable);
}

void ABaseDoor::DoorMovement(float DeltaTime)
{
	const double SafeMoveTime = FMath::Max(static_cast<double>(this->DoorMoveTime), 0.01);

	// 열 때 증가, 닫을 때 감소
	const double Direction = this->bIsOpen ? 1.0 : -1.0;

	this->OpenAlpha = FMath::Clamp(
		this->OpenAlpha + Direction * DeltaTime / SafeMoveTime,
		0.0,
		1.0);
}

void	ABaseDoor::ResetInteractionCooldown()
{
	this->bIsInteractCooldown = false;
}
