// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjComponent/TargetingComponent.h"

// Sets default values for this component's properties
UTargetingComponent::UTargetingComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UTargetingComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

bool UTargetingComponent::BeginTargeting()
{
	if (this->bIsSelecting)
		return (false);
	this->bIsSelecting = true;
	if (GetOwner()->GetInstigatorController())
	PrimaryComponentTick.bCanEverTick = true;
	this->OnTargetingStarted();
	return (true);
}

void UTargetingComponent::AcceptTargeting()
{
	if (!this->BeginTargeting())
		return ;
}

void UTargetingComponent::CancelTargeting()
{
}

void UTargetingComponent::EndTargeting()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTargetingComponent::OnTargetingStarted()
{
}

void UTargetingComponent::UpdateTargeting(float DeltaTime)
{
}

bool UTargetingComponent::CanConfirmTargeting() const
{
	return false;
}

void UTargetingComponent::OnTargetingConfirmed()
{
}

void UTargetingComponent::OnTargetingEnded()
{
}

