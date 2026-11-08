


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

void ASlidingDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	this->RestorePreviewLocation();
	if (!this->bStartOpen)
	{
		return;
	}

	this->RightPreviewOffset = this->RightDoorOpenOffset;
	this->LeftPreviewOffset = this->LeftDoorOpenOffset;
	this->RightDoorMesh->SetRelativeLocation(this->RightDoorMesh->GetRelativeLocation() + this->RightPreviewOffset);
	this->LeftDoorMesh->SetRelativeLocation(this->LeftDoorMesh->GetRelativeLocation() + this->LeftPreviewOffset);
	this->bPreviewOpen = true;
}

void ASlidingDoor::RestorePreviewLocation()
{
	if (!this->bPreviewOpen)
	{
		return;
	}

	// 미리보기 중에 메시를 끌어 옮겼어도 그만큼 닫힌 위치가 같이 옮겨진다
	this->RightDoorMesh->SetRelativeLocation(this->RightDoorMesh->GetRelativeLocation() - this->RightPreviewOffset);
	this->LeftDoorMesh->SetRelativeLocation(this->LeftDoorMesh->GetRelativeLocation() - this->LeftPreviewOffset);
	this->bPreviewOpen = false;
}

void ASlidingDoor::BeginPlay()
{
	Super::BeginPlay();

	// 메시는 첫 Tick의 DoorMovement가 열린 위치로 다시 옮긴다
	this->RestorePreviewLocation();

	this->RightDoorCloseLocation = this->RightDoorMesh->GetRelativeLocation();
	this->LeftDoorCloseLocation = this->LeftDoorMesh->GetRelativeLocation();

	this->RightDoorOpenLocation = this->RightDoorCloseLocation + this->RightDoorOpenOffset;
	this->LeftDoorOpenLocation = this->LeftDoorCloseLocation + this->LeftDoorOpenOffset;
}
