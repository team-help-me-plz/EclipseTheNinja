// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzelObject/PressureSwitch.h"
#include "Interface/Interactor.h"
#include "Checkpoint/CheckpointBoundComponent.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

APressureSwitch::APressureSwitch()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	this->PressureTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("PressureTrigger"));

	// 발판 메시의 스케일에 따라 박스 크기가 찌그러지지 않도록 설정
	this->PressureTrigger->SetAbsolute(false, false, true);

	// Extent는 전체 크기의 절반: 실제 크기는 150 × 150 × 100
	this->PressureTrigger->SetBoxExtent(FVector(75.0f, 75.0f, 50.0f));

	this->PressureTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	this->PressureTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	this->PressureTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	this->PressureTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	this->PressureTrigger->SetGenerateOverlapEvents(true);
	SetRootComponent(this->PressureTrigger);

	this->Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	this->Mesh->SetMobility(EComponentMobility::Movable);
	this->Mesh->SetupAttachment(this->PressureTrigger);

	this->CheckpointBound = CreateDefaultSubobject<UCheckpointBoundComponent>(TEXT("CheckpointBound"));
}

void APressureSwitch::ResetToInitial_Implementation()
{
	this->bIsInteracted = false;
}

void APressureSwitch::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void APressureSwitch::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	this->PressureTrigger->OnComponentBeginOverlap.AddDynamic(this, &APressureSwitch::OnPressureBeginOverlap);
	this->PressureTrigger->OnComponentEndOverlap.AddDynamic(this, &APressureSwitch::OnPressureEndOverlap);
}

void APressureSwitch::OnPressureBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 한 번만 눌리는 발판이고 이미 눌렸을 때 아무것도 하지 않는다
	if (!this->bIsUnpressure && !this->bIsReinteractable && this->bIsInteracted)
	{
		return ;
	}
	IInteractable::Execute_Interact(this, OtherActor);
}

void APressureSwitch::OnPressureEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!this->bIsUnpressure)
	{
		return;
	}
	IInteractable::Execute_Interact(this, OtherActor);
}

void APressureSwitch::Interact_Implementation(AActor* InActor)
{
	if (!IsValid(InActor) || !InActor->Implements<UInteractor>())
	{
		return;
	}
	if (!this->TargetActor || !this->TargetActor->Implements<UActivatable>())
	{
		return;
	}
	IActivatable::Execute_ToggleActivate(this->TargetActor);
	this->bIsInteracted = true;
}
void APressureSwitch::ShowInteractionIndicator_Implementation()
{
}

void APressureSwitch::HideInteractionIndicator_Implementation()
{
}

bool APressureSwitch::bIsInteractReady_Implementation()
{
	return (false);
}





