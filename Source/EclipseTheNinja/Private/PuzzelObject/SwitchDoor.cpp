// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzelObject/SwitchDoor.h"
#include "Interface/Interactor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ASwitchDoor::ASwitchDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	this->Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	this->Mesh->SetMobility(EComponentMobility::Movable);
	SetRootComponent(this->Mesh);
}

void ASwitchDoor::BeginPlay()
{
	Super::BeginPlay();

	this->CloseLocation = GetActorLocation();
	this->OpenLocation = GetActorLocation() + this->OpenOffset;
	this->OpenAlpha = 0.0;
}

void ASwitchDoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	this->DoorMovement(DeltaTime);
}

void ASwitchDoor::ToggleActivate_Implementation()
{
	this->bIsOpen = !this->bIsOpen;
}

void ASwitchDoor::Interact_Implementation(AActor* InActor)
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
		&ASwitchDoor::ResetInteractionCooldown,
		this->InteractInterval,
		false
	);
	this->bIsOpen = !this->bIsOpen;
}

void ASwitchDoor::ShowInteractionIndicator_Implementation()
{
	if (!this->bIsInteractable)
	{
		return;
	}
}

void ASwitchDoor::HideInteractionIndicator_Implementation()
{
	if (!this->bIsInteractable)
	{
		return;
	}
}

bool ASwitchDoor::bIsInteractReady_Implementation()
{
	return (this->bIsInteractable);
}

void ASwitchDoor::DoorMovement(float DeltaTime)
{
	const double SafeMoveTime = FMath::Max(static_cast<double>(this->DoorMoveTime), 0.01);

	// 열 때 증가, 닫을 때 감소
	const double Direction = this->bIsOpen ? 1.0 : -1.0;

	this->OpenAlpha = FMath::Clamp(
		this->OpenAlpha + Direction * DeltaTime / SafeMoveTime,
		0.0,
		1.0);

	// 양 끝 위치는 항상 고정
	const FVector NewLocation = FMath::Lerp(
		this->CloseLocation,
		this->OpenLocation,
		this->OpenAlpha);

	SetActorLocation(NewLocation);
}

void	ASwitchDoor::ResetInteractionCooldown()
{
	this->bIsInteractCooldown = false;
}
