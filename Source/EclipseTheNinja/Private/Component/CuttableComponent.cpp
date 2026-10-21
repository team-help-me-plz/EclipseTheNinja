


#include "Component/CuttableComponent.h"

UCuttableComponent::UCuttableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCuttableComponent::CutRope()
{
	if (this->bIsCutted)
	{
		return ;
	}
	this->bIsCutted = true;
	this->SetVisibility(false);
	this->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	this->OnRopeCut.Broadcast();
}

void UCuttableComponent::RestoreRope()
{
	this->bIsCutted = false;
	this->SetVisibility(true);
	this->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void UCuttableComponent::SetRopeLength(const float InLength)
{
}
