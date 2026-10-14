// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interface/Activatable.h"
#include "Interface/Interactable.h"

#include "GameFramework/Actor.h"
#include "BaseDoor.generated.h"

class UStaticMeshComponent;

UCLASS()
class ECLIPSETHENINJA_API ABaseDoor : public AActor, public IActivatable, public IInteractable
{
	GENERATED_BODY()

public:
	ABaseDoor();

	virtual void	Tick(float DeltaTime) override;

	virtual void	ToggleActivate_Implementation() override;

	virtual void	Interact_Implementation(AActor* InActor) override;
	virtual void	ShowInteractionIndicator_Implementation() override;
	virtual void	HideInteractionIndicator_Implementation() override;
	virtual bool	bIsInteractReady_Implementation() override;

	virtual void	DoorMovement(float DeltaTime);

protected:
	virtual void	BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent>			RootScene = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMeshComponent>	RightDoorMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMeshComponent>	LeftDoorMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.01"))
	float	DoorMoveTime = 3.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Interactable", meta = (ClampMin = "0.01"))
	bool	bIsInteractable = true;

	UPROPERTY(Transient)
	double	OpenAlpha = 0.0;

	UPROPERTY(Transient)
	bool	bIsOpen = false;

private:
	void	ResetInteractionCooldown();


	FTimerHandle	InteractionCooldownTimer;
	bool			bIsInteractCooldown = false;
	float			InteractInterval = 0.5f;
};
