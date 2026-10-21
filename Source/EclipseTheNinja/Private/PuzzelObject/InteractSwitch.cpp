// Fill out your copyright notice in the Description page of Project Settings.


#include "PuzzelObject/InteractSwitch.h"
#include "Interface/Interactor.h"
#include "Interface/Activatable.h"
#include "Util/MeshAssembly.h"
#include "Checkpoint/CheckpointBoundComponent.h"
#include "Component/InteractHighlightComponent.h"

#include "Components/SphereComponent.h"

// Sets default values
AInteractSwitch::AInteractSwitch()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//this->InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	//this->InteractSphere->SetSphereRadius(this->InteractRadius);
	//SetRootComponent(this->InteractSphere);

	this->RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(this->RootScene);

	this->MeshAssembly = CreateDefaultSubobject<UChildActorComponent>(TEXT("MeshAssembly"));
	this->MeshAssembly->SetupAttachment(GetRootComponent());

	this->CheckpointBound = CreateDefaultSubobject<UCheckpointBoundComponent>(TEXT("CheckpointBound"));
	this->InteractHighlight = CreateDefaultSubobject<UInteractHighlightComponent>(TEXT("InteractHighlight"));
}

void AInteractSwitch::ResetToInitial_Implementation()
{
	this->bIsInteracted = false;

	GetWorldTimerManager().ClearTimer(this->InteractionCooldownTimer);
	this->bIsInteractCooldown = false;
	this->RefreshInteractState();
}

// Called when the game starts or when spawned
void AInteractSwitch::BeginPlay()
{
	Super::BeginPlay();

	this->RefreshInteractState();

	AActor* AssemblyActor = this->MeshAssembly->GetChildActor();
	if (!IsValid(AssemblyActor))
	{
		return;
	}

	TArray<UStaticMeshComponent*> Meshes;
	AssemblyActor->GetComponents<UStaticMeshComponent>(Meshes);

	for (UStaticMeshComponent* Mesh : Meshes)
	{
		Mesh->SetCollisionObjectType(ECC_WorldDynamic);

		Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
}

// Called every frame
void AInteractSwitch::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AInteractSwitch::Interact_Implementation(AActor* InActor)
{
	if (!IsValid(InActor) || !InActor->Implements<UInteractor>())
	{
		return;
	}
	if (!this->TargetActor || !this->TargetActor->Implements<UActivatable>())
	{
		return;
	}
	// 한 번만 눌리는 발판이고 이미 눌렸을 때, 그리고 상호작용 쿨타임일 때
	if (bIsInteractCooldown || (!bIsReinteractable && bIsInteracted))
	{
		return;
	}
	this->bIsInteractCooldown = true;
	this->bIsInteracted = true;
	GetWorldTimerManager().SetTimer(
		InteractionCooldownTimer,
		this,
		&AInteractSwitch::ResetInteractionCooldown,
		this->InteractInterval,
		false
	);
	this->RefreshInteractState();
	IActivatable::Execute_ToggleActivate(this->TargetActor);
}

void AInteractSwitch::ShowInteractionIndicator_Implementation()
{
	this->SelectCount++;
	this->RefreshHighlight();
}

void AInteractSwitch::HideInteractionIndicator_Implementation()
{
	this->SelectCount = FMath::Max(0, this->SelectCount - 1);
	this->RefreshHighlight();
}

bool AInteractSwitch::bIsInteractReady_Implementation()
{
	return (this->InteractState == EInteractState::Ready);
}

EInteractState AInteractSwitch::GetInteractState_Implementation()
{
	return (this->InteractState);
}

void AInteractSwitch::ResetInteractionCooldown()
{
	this->bIsInteractCooldown = false;
	this->RefreshInteractState();
}

void AInteractSwitch::RefreshInteractState()
{
	// 한 번만 누를 수 있는 스위치가 이미 눌렸으면 쿨다운 여부와 관계없이 Done
	if (!this->bIsReinteractable && this->bIsInteracted)
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

void AInteractSwitch::RefreshHighlight()
{
	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetHighlight(this->InteractState, (this->SelectCount > 0));
	}

	// 선택되어 있는 동안 이 스위치가 작동시킬 대상의 테두리도 켠다
	const bool	bShouldShowTarget = (this->SelectCount > 0) && this->InteractState != EInteractState::Done;
	if (bShouldShowTarget == this->bIsShowingTargetIndicator)
	{
		return ;
	}
	this->bIsShowingTargetIndicator = bShouldShowTarget;
	if (!IsValid(this->TargetActor) || !this->TargetActor->Implements<UActivatable>())
	{
		return ;
	}
	if (bShouldShowTarget)
	{
		IActivatable::Execute_ShowActivationIndicator(this->TargetActor);
	}
	else
	{
		IActivatable::Execute_HideActivationIndicator(this->TargetActor);
	}
}



