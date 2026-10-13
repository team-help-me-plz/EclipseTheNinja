// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interface/Interactable.h"
#include "Interface/Activatable.h"

#include "GameFramework/Actor.h"
#include "PressureSwitch.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class ECLIPSETHENINJA_API APressureSwitch : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	APressureSwitch();

	virtual void	Tick(float DeltaTime) override;

	virtual void	Interact_Implementation(AActor* InActor) override;
	virtual void	ShowInteractionIndicator_Implementation() override;
	virtual void	HideInteractionIndicator_Implementation() override;
	virtual bool	bIsInteractReady_Implementation() override;

protected:
	virtual void PostInitializeComponents() override;

	UFUNCTION()
	void OnPressureBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnPressureEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent>	Mesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UBoxComponent>	PressureTrigger = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interact|Target")
	TObjectPtr<AActor>			TargetActor = nullptr;

	// PressureTrigger 밖으로 플레이어 혹은 분신이 나갔을 때 TargetActor의 Activate 상황을 다시 토글할 것인가
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interact|State")
	bool	bIsUnpressure = false;

private:
	// Interact 함수가 한 번이라도 불렸는가
	bool	bIsInteracted = false;
};


