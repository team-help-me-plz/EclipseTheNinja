// Fill out your copyright notice in the Description page of Project Settings.

#include "MapObject/PressureSwitch.h"
#include "MapObject/SwitchDoor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

APressureSwitch::APressureSwitch()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	UStaticMeshComponent*	Mesh = GetStaticMeshComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 0.15f));
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh>	CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}

	this->PressureTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("PressureTrigger"));
	this->PressureTrigger->SetupAttachment(Mesh);
	this->PressureTrigger->SetAbsolute(false, false, true);
	this->PressureTrigger->SetRelativeLocation(FVector::ZeroVector);
	this->PressureTrigger->SetBoxExtent(FVector(75.0f, 75.0f, 50.0f));
	this->PressureTrigger->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	this->PressureTrigger->SetGenerateOverlapEvents(true);
}

void APressureSwitch::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsValid(this->TargetDoor.Get()))
	{
		return;
	}

	// 플레이어와 Pawn을 상속한 분신을 동일하게 감지합니다.
	TArray<AActor*>	OverlappingActors;
	GetOverlappingActors(OverlappingActors, APawn::StaticClass());

	this->TargetDoor->SetOpen(!OverlappingActors.IsEmpty());
}
