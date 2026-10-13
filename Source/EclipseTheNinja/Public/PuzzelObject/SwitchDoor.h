// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interface/Activatable.h"
#include "Interface/Interactable.h"

#include "GameFramework/Actor.h"
#include "SwitchDoor.generated.h"

class UStaticMeshComponent;

UCLASS()
class ECLIPSETHENINJA_API ASwitchDoor : public AActor, public IActivatable, public IInteractable
{
	GENERATED_BODY()

public:
	ASwitchDoor();

	virtual void	Tick(float DeltaTime) override;

	virtual void	ToggleActivate_Implementation() override;

	virtual void	Interact_Implementation(AActor* InActor) override;
	virtual void	ShowInteractionIndicator_Implementation() override;
	virtual void	HideInteractionIndicator_Implementation() override;
	virtual bool	bIsInteractReady_Implementation() override;

	void	DoorMovement(float DeltaTime);

protected:
	virtual void	BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent>	Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FVector	OpenOffset = FVector(0.0f, 0.0f, 280.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.01"))
	float	DoorMoveTime = 3.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Interactable", meta = (ClampMin = "0.01"))
	bool	bIsInteractable = true;

private:
	UPROPERTY(Transient)
	double	ElapsedTime = 0.0;

	UPROPERTY(Transient)
	FVector	CloseLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector	OpenLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	double	OpenAlpha = 0.0;

	UPROPERTY(Transient)
	bool	bIsOpen = false;

};
