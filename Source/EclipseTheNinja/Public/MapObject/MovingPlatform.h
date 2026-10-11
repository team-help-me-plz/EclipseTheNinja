// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "MovingPlatform.generated.h"

UCLASS()
class ECLIPSETHENINJA_API AMovingPlatform : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	AMovingPlatform();

	virtual void	Tick(float DeltaTime) override;

protected:
	virtual void	BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Gameplay")
	FVector	MoveOffset = FVector(400.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Gameplay", meta = (ClampMin = "0.1"))
	double	TravelTime = 4.0;

private:
	UPROPERTY(Transient)
	FVector	StartLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	double	ElapsedTime = 0.0;
};
