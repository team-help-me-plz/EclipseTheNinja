


#include "PuzzelObject/SlidingDoor.h"
#include "Components/StaticMeshComponent.h"

void ASlidingDoor::DoorMovement(float DeltaTime)
{
	Super::DoorMovement(DeltaTime);

	const FVector	RightNewLocation = FMath::Lerp(
		this->RightDoorCloseLocation,
		this->RightDoorOpenLocation,
		this->OpenAlpha);

	const FVector	LeftNewLocation = FMath::Lerp(
		this->LeftDoorCloseLocation,
		this->LeftDoorOpenLocation,
		this->OpenAlpha);

	this->RightDoorMesh->SetRelativeLocation(RightNewLocation);
	this->LeftDoorMesh->SetRelativeLocation(LeftNewLocation);
}

void ASlidingDoor::BeginPlay()
{
	Super::BeginPlay();

	this->RightDoorCloseLocation = this->RightDoorMesh->GetRelativeLocation();
	this->LeftDoorCloseLocation = this->LeftDoorMesh->GetRelativeLocation();

	this->RightDoorOpenLocation = this->RightDoorCloseLocation + this->RightDoorOpenOffset;
	this->LeftDoorOpenLocation = this->LeftDoorCloseLocation + this->LeftDoorOpenOffset;
}
