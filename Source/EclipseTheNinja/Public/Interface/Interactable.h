// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

class AActor;

// 상호작용 액터의 현재 상태
UENUM(BlueprintType)
enum class EInteractState : uint8
{
	Ready	,
	Cooldown,
	Done	,
};

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ECLIPSETHENINJA_API IInteractable
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	// 상호작용 시작 함수
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void	Interact(AActor* InActor);
	// 상호작용 액터의 하이라이트를 켜는 함수
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void	ShowInteractionIndicator();
	// 상호작용 액터의 하이라이트를 끄는 함수
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void	HideInteractionIndicator();
	// 상호작용 가능 여부를 확인하는 함수
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	bool	bIsInteractReady();
	// 상호작용 상태를 확인하는 함수
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	EInteractState	GetInteractState();
};
