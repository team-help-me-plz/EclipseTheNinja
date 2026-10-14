


#include "PuzzelObject/HingedDoor.h"

void AHingedDoor::DoorMovement(float DeltaTime)
{
	Super::DoorMovement(DeltaTime);

	const FRotator	RightNewLocation = FMath::Lerp(
		FRotator(0.f, 0.f, 0.f),
		this->RightDoorOpenOffset,
		this->OpenAlpha);

	const FRotator	LeftNewLocation = FMath::Lerp(
		FRotator(0.f, 0.f, 0.f),
		this->LeftDoorOpenOffset,
		this->OpenAlpha);

	this->RightDoorMesh->SetRelativeRotation(RightNewLocation);
	this->LeftDoorMesh->SetRelativeRotation(LeftNewLocation);
}

void AHingedDoor::BeginPlay()
{
	Super::BeginPlay();
}
