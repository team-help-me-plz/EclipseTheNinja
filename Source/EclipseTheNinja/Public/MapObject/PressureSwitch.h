// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "PressureSwitch.generated.h"

class ASwitchDoor;
class UBoxComponent;

UCLASS()
class ECLIPSETHENINJA_API APressureSwitch : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	APressureSwitch();

	virtual void	Tick(float DeltaTime) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Gameplay")
	TObjectPtr<UBoxComponent>	PressureTrigger = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Map Gameplay")
	TObjectPtr<ASwitchDoor>		TargetDoor = nullptr;
};
