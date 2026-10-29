// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzelObject/BaseDoor.h"
#include "Interface/Interactor.h"
#include "Checkpoint/CheckpointBoundComponent.h"
#include "Component/InteractHighlightComponent.h"
#include "Component/ActivateHighlightComponent.h"
#include "Subsystem/UIMessageSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
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

	this->CheckpointBound = CreateDefaultSubobject<UCheckpointBoundComponent>(TEXT("CheckpointBound"));
	this->InteractHighlight = CreateDefaultSubobject<UInteractHighlightComponent>(TEXT("InteractHighlight"));
	this->ActivateHighlight = CreateDefaultSubobject<UActivateHighlightComponent>(TEXT("ActivateHighlight"));

	this->InteractKeyWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractKeyWidget"));
	this->InteractKeyWidget->SetupAttachment(GetRootComponent());
	this->InteractKeyWidget->SetWidgetSpace(EWidgetSpace::Screen);
	this->InteractKeyWidget->SetDrawAtDesiredSize(true);
	this->InteractKeyWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	this->InteractKeyWidget->SetVisibility(false);
}

void ABaseDoor::BeginPlay()
{
	Super::BeginPlay();

	//this->CloseLocation = GetActorLocation();
	//this->OpenLocation = GetActorLocation() + this->OpenOffset;
	this->OpenAlpha = 0.0;
	this->bInitialIsOpen = this->bIsOpen;
	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetKeyWidget(this->InteractKeyWidget);
	}
	this->RefreshInteractState();
}

void ABaseDoor::ResetToInitial_Implementation()
{
	this->bIsOpen = this->bInitialIsOpen;
	this->OpenAlpha = this->bInitialIsOpen ? 1.0 : 0.0;

	GetWorldTimerManager().ClearTimer(this->InteractionCooldownTimer);
	this->bIsInteractCooldown = false;
	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetKeyWidget(this->InteractKeyWidget);
	}
	this->RefreshInteractState();
}

void ABaseDoor::OnAfterReset_Implementation()
{
	// DeltaTime 0 으로 호출하면 OpenAlpha는 그대로 두고 메시 위치/회전만 갱신된다
	this->DoorMovement(0.f);
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

void ABaseDoor::ShowActivationIndicator_Implementation()
{
	this->ActivateHighlight->ShowHighlight();
}

void ABaseDoor::HideActivationIndicator_Implementation()
{
	this->ActivateHighlight->HideHighlight();
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
	if (this->LockedVineCount != 0)
	{
		APlayerController* PC = Cast<APlayerController>(InActor->GetInstigatorController());
		if (PC)
		{
			UUIMessageSubsystem::ShowMessage(PC, FText::FromString(TEXT("덩굴에 감겨있습니다.")));
		}
		return;
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
	this->RefreshInteractState();
}

void ABaseDoor::ShowInteractionIndicator_Implementation()
{
	this->SelectCount++;
	this->RefreshHighlight();
}

void ABaseDoor::HideInteractionIndicator_Implementation()
{
	this->SelectCount = FMath::Max(0, this->SelectCount - 1);
	this->RefreshHighlight();
}

bool ABaseDoor::bIsInteractReady_Implementation()
{
	return (this->InteractState == EInteractState::Ready);
}

EInteractState ABaseDoor::GetInteractState_Implementation()
{
	return (this->InteractState);
}

void ABaseDoor::RefreshInteractState()
{
	// 직접 상호작용할 수 없는 문(스위치 등으로만 열리는 문)은 Done
	if (!this->bIsInteractable)
	{
		this->InteractState = EInteractState::Done;
	}
	else if (this->bIsInteractCooldown)
	{
		this->InteractState = EInteractState::Cooldown;
	}
	else
	{
		this->InteractState = EInteractState::Ready;
	}
	this->RefreshHighlight();
}

void ABaseDoor::RefreshHighlight()
{
	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetHighlight(this->InteractState, (this->SelectCount > 0));
	}
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

void ABaseDoor::AddInteractLock_Implementation()
{
	this->LockedVineCount++;
}

void ABaseDoor::RemoveInteractLock_Implementation()
{
	this->LockedVineCount--;
}

bool ABaseDoor::IsInteractLocked_Implementation()
{
	return (this->LockedVineCount > 0);
}

void	ABaseDoor::ResetInteractionCooldown()
{
	this->bIsInteractCooldown = false;
	this->RefreshInteractState();
}
