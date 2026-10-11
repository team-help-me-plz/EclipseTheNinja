// Fill out your copyright notice in the Description page of Project Settings.

#include "MapObject/MovingPlatform.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AMovingPlatform::AMovingPlatform()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	UStaticMeshComponent*	Mesh = GetStaticMeshComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetRelativeScale3D(FVector(3.0f, 3.0f, 0.25f));
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh>	CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AMovingPlatform::BeginPlay()
{
	Super::BeginPlay();

	this->StartLocation = GetActorLocation();
	this->ElapsedTime = 0.0;
}

void AMovingPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	this->ElapsedTime += static_cast<double>(DeltaTime);

	const double	Period = FMath::Max(this->TravelTime, 0.1);
	const double	Phase = this->ElapsedTime / Period * 6.283185307179586;
	const float		Alpha = static_cast<float>(FMath::Cos(Phase) * -0.5 + 0.5);
	const FVector	EndLocation = this->StartLocation + this->MoveOffset;

	// 블루프린트와 동일하게 월드 좌표 기준으로 왕복합니다.
	SetActorLocation(FMath::Lerp(this->StartLocation, EndLocation, Alpha), false);
}
