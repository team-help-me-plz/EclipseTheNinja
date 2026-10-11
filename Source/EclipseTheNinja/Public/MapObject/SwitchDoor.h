// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "SwitchDoor.generated.h"

UCLASS()
class ECLIPSETHENINJA_API ASwitchDoor : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	ASwitchDoor();

	virtual void	Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Map Gameplay")
	void	SetOpen(bool bInOpen);

	UFUNCTION(BlueprintPure, Category = "Map Gameplay")
	bool	IsOpen() const;

protected:
	virtual void	BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Gameplay")
	FVector	OpenOffset = FVector(0.0f, 0.0f, 280.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Gameplay", meta = (ClampMin = "0.01"))
	double	OpenSpeed = 3.0;

private:
	UPROPERTY(Transient)
	FVector	StartLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	double	OpenAlpha = 0.0;

	UPROPERTY(Transient)
	bool	bOpen = false;
};
