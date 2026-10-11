// Fill out your copyright notice in the Description page of Project Settings.

#include "MapObject/SwitchDoor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ASwitchDoor::ASwitchDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	UStaticMeshComponent*	Mesh = GetStaticMeshComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetRelativeScale3D(FVector(1.5f, 0.25f, 2.5f));
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh>	CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
}

void ASwitchDoor::BeginPlay()
{
	Super::BeginPlay();

	this->StartLocation = GetActorLocation();
	this->OpenAlpha = 0.0;
}

void ASwitchDoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const double	TargetAlpha = this->bOpen ? 1.0 : 0.0;
	this->OpenAlpha = FMath::FInterpTo(this->OpenAlpha, TargetAlpha, static_cast<double>(DeltaTime), this->OpenSpeed);

	const FVector	EndLocation = this->StartLocation + this->OpenOffset;
	const float		Alpha = static_cast<float>(this->OpenAlpha);

	SetActorLocation(FMath::Lerp(this->StartLocation, EndLocation, Alpha), false);
}

void ASwitchDoor::SetOpen(bool bInOpen)
{
	this->bOpen = bInOpen;
}

bool ASwitchDoor::IsOpen() const
{
	return this->bOpen;
}
