// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interface/Interactable.h"

#include "GameFramework/Actor.h"
#include "InteractSwitch.generated.h"

class AMeshAssembly;
class USphereComponent;

UCLASS()
class ECLIPSETHENINJA_API AInteractSwitch : public AActor, public IInteractable
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AInteractSwitch();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void	Interact_Implementation(AActor* InActor) override;
	virtual void	ShowInteractionIndicator_Implementation() override;
	virtual void	HideInteractionIndicator_Implementation() override;
	virtual bool	bIsInteractReady_Implementation() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent>			RootScene = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UChildActorComponent>	MeshAssembly = nullptr;
	
	//UPROPERTY(EditAnywhere, BlueprintReadOnly)
	//TObjectPtr<USphereComponent>	InteractSphere = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interact|Target")
	TObjectPtr<AActor>			TargetActor = nullptr;

	// 여러번 Interact 할 수 있는 액터인가
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interact|State")
	bool	bIsReinteractable = false;
	
	//// 디폴트 맵의 Y축 중앙이 300임을 이용(700.f~1300.f)해 500.f로 일단 설정함
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact|Radius")
	//float	InteractRadius = 500.f;

private:
	void	ResetInteractionCooldown();

	// Interact 함수가 한 번이라도 불렸는가
	bool	bIsInteracted = false;

	FTimerHandle	InteractionCooldownTimer;
	bool			bIsInteractCooldown = false;
	float			InteractInterval = 0.5f;
};
