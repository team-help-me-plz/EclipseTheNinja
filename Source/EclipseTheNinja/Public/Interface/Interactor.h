// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactor.generated.h"

class ATargetPoint;
class UAnimMontage;

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
	void	RegisterSafePoint(const ATargetPoint* InSafePoint);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void	ReturnToSafePoint();

	// 상호작용 성공 시 상호작용 액터가 호출. InMontage가 nullptr이면 Interactor의 기본 몽타주를 재생
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void	PlayInteractMontage(UAnimMontage* InMontage);

};
