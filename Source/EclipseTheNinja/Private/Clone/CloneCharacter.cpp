// Fill out your copyright notice in the Description page of Project Settings.


#include "Clone/CloneCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
ACloneCharacter::ACloneCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	UCharacterMovementComponent* Movement = GetCharacterMovement();

	Movement->bRunPhysicsWithNoController = true;
	Movement->MaxWalkSpeed = 250.f;

	Movement->SetPlaneConstraintNormal(FVector::YAxisVector);
	Movement->bConstrainToPlane = true;
}

// Called when the game starts or when spawned
void ACloneCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->SetPlaneConstraintOrigin(GetActorLocation());

	const float	Sign = GetActorForwardVector().X >= 0.f ? 1.f : -1.f;
	this->WalkDir = FVector(Sign, 0.f, 0.f);

	SetLifeSpan(5.f);
}

// Called every frame
void ACloneCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	this->LifeTime += DeltaTime;
	if (this->LifeTime > this->DeadTime)
	{
		Destroy();
	}
	AddMovementInput(this->WalkDir, 1.f, true);
}

// Called to bind functionality to input
void ACloneCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

