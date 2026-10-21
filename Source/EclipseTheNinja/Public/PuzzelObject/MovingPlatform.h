// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interface/Activatable.h"
#include "Interface/CheckpointResettable.h"

#include "Engine/TargetPoint.h"
#include "GameFramework/Actor.h"
#include "MovingPlatform.generated.h"

class UStaticMeshComponent;
class UCheckpointBoundComponent;
class UActivateHighlightComponent;

UCLASS()
class ECLIPSETHENINJA_API AMovingPlatform : public AActor, public IActivatable, public ICheckpointResettable
{
	GENERATED_BODY()

public:
	AMovingPlatform();

	virtual void	Tick(float DeltaTime) override;

	virtual void	ToggleActivate_Implementation() override;
	virtual void	ShowActivationIndicator_Implementation() override;
	virtual void	HideActivationIndicator_Implementation() override;

	virtual void ResetToInitial_Implementation() override;
	virtual void OnAfterReset_Implementation() override;

protected:
	virtual void	BeginPlay() override;

	void	PlatformMovement(float DeltaTime);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent>			RootScene = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UChildActorComponent>	MeshAssembly = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Movement")
	TArray<ATargetPoint*>	TargetPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.1"))
	float					TargetPointTravelTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCheckpointBoundComponent>	CheckpointBound = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UActivateHighlightComponent>	ActivateHighlight = nullptr;

	// 트리거로 움직임을 제어할 수 있는지 설정하는 값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|State")
	bool	bIsActivatable = false;

	// 움직임을 제어하는 값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|State")
	bool	bIsMovement = true;

	// bIsMovement가 false일 때 원위치로 돌아갈지 설정하는 값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|State")
	bool	bIsResetable = true;

private:
	float	RouteProgress = 0.0;
	float	EndPointFloat = 0.0;
	float	CycleLength = 0.0;
	
	bool	bIsMoveForward = true;

	bool	bIsMovementUse = true;
};
