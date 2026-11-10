


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

void AHingedDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 닫힌 각도는 DoorMovement와 같이 0 기준. 미리보기를 쓴 적 없는 문은 건드리지 않는다
	if (this->bStartOpen)
	{
		this->RightDoorMesh->SetRelativeRotation(this->RightDoorOpenOffset);
		this->LeftDoorMesh->SetRelativeRotation(this->LeftDoorOpenOffset);
	}
	else if (this->bPreviewOpen)
	{
		this->RightDoorMesh->SetRelativeRotation(FRotator::ZeroRotator);
		this->LeftDoorMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}
	this->bPreviewOpen = this->bStartOpen;
}

void AHingedDoor::BeginPlay()
{
	Super::BeginPlay();
}
