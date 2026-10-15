// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactor.generated.h"

class ATargetPoint;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UInteractor : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ECLIPSETHENINJA_API IInteractor
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void	RegisterSafePoint(const FVector& InSafePoint);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void	ReturnToSafePoint();

};
