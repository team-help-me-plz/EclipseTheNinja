// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Clone/CloneCommandType.h"
#include "Interface/Interactable.h"
#include "Interface/Interactor.h"

#include "GameFramework/Character.h"
#include "CloneCharacter.generated.h"

UCLASS()
class ECLIPSETHENINJA_API ACloneCharacter : public ACharacter, public IInteractor, public IInteractable
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

	virtual void	Interact_Implementation(AActor* InActor) override;
	virtual void	ShowInteractionIndicator_Implementation() override;
	virtual void	HideInteractionIndicator_Implementation() override;
	virtual bool	bIsInteractReady_Implementation() override;

	virtual void	RegisterSafePoint_Implementation(const FVector& InSafePoint) override;
	virtual void	ReturnToSafePoint_Implementation() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Command|Margin")
	float	TargetLocationMargin = 49.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact|Margin")
	FVector	InteractVelocity = FVector(0.f, 0.f, 500.f);

private:
	void	ResetInteractionCooldown();

	bool	bIsArrived = false;
	FVector	WalkDir = FVector::ZeroVector;

	TObjectPtr<AActor>	TargetActor = nullptr;
	FVector				TargetLocation = FVector::ZeroVector;
	ECloneCommandType	CommandType;

	FTimerHandle	InteractionCooldownTimer;
	bool			bIsInteractCooldown = false;
	float			InteractInterval = 0.5f;

	FVector	SafePointLocation = FVector::ZeroVector;
};
