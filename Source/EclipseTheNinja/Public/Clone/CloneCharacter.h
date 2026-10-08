// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Clone/CloneCommandType.h"
#include "Interface/Interactable.h"

#include "GameFramework/Character.h"
#include "CloneCharacter.generated.h"

UCLASS()
class ECLIPSETHENINJA_API ACloneCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ACloneCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void	SetCommand(const FCloneCommandType& InCommand);

private:
	bool	bIsArrived = false;
	FVector	WalkDir = FVector::ZeroVector;

	TObjectPtr<AActor>	TargetActor = nullptr;
	FVector				TargetLocation = FVector::ZeroVector;
	ECloneCommandType	CommandType;
};
