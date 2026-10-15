// Fill out your copyright notice in the Description page of Project Settings.


#include "PuzzelObject/InteractSwitch.h"
#include "Interface/Interactor.h"
#include "Interface/Activatable.h"
#include "Util/MeshAssembly.h"

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

}

// Called when the game starts or when spawned
void AInteractSwitch::BeginPlay()
{
	Super::BeginPlay();
	
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
	IActivatable::Execute_ToggleActivate(this->TargetActor);
}

void AInteractSwitch::ShowInteractionIndicator_Implementation()
{
}

void AInteractSwitch::HideInteractionIndicator_Implementation()
{
}

bool AInteractSwitch::bIsInteractReady_Implementation()
{
	return (!this->bIsInteractCooldown && (this->bIsReinteractable || !this->bIsInteracted));
}

void AInteractSwitch::ResetInteractionCooldown()
{
	this->bIsInteractCooldown = false;
}



