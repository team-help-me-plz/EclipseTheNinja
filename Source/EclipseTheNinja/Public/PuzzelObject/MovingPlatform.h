// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interface/Activatable.h"

#include "Engine/TargetPoint.h"
#include "GameFramework/Actor.h"
#include "MovingPlatform.generated.h"

class UStaticMeshComponent;

UCLASS()
class ECLIPSETHENINJA_API AMovingPlatform : public AActor, public IActivatable
{
	GENERATED_BODY()

public:
	AMovingPlatform();

	virtual void	Tick(float DeltaTime) override;

	virtual void	ToggleActivate_Implementation() override;

protected:
	virtual void	BeginPlay() override;

	void	PlatformMovement(float DeltaTime);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent>	Mesh = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Movement")
	TArray<ATargetPoint*>	TargetPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.1"))
	double					TargetPointTravelTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|State")
	bool	bIsActivatable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|State")
	bool	bIsMovement = true;

private:
	UPROPERTY(Transient)
	double	ElapsedTime = 0.0;

	int32	CurrentTargetPoint = 0;
	int32	NextTargetPoint = 1;
	bool	bIsMoveForward = true;
};
