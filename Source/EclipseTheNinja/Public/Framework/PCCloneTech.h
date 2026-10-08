// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Variant_SideScrolling/SideScrollingPlayerController.h"
#include "PCCloneTech.generated.h"

//DECLARE_DELEGATE_OneParam(FOnTargetingAccept, );
//DECLARE_DELEGATE_OneParam(FOnTargetingCancel, );

class UTargetingComponent;
class UGATargetingAbility;

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API APCCloneTech : public ASideScrollingPlayerController
{
	GENERATED_BODY()
	
public:
	bool	bIsTargeting() { return (this->SavedTargetingComponent ? true : false); }
	void	StartTargeting(UTargetingComponent* InTargetingComponent, UGATargetingAbility* InGameplayAbility);
	void	EndTargeting();

	void	DoAccept();
	void	DoCancel();

protected:
	UPROPERTY(EditAnywhere, Category = "Input|Ability Select")
	TObjectPtr<UInputMappingContext>	TargetingMappingContext = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Component|Save")
	TObjectPtr<UTargetingComponent>		SavedTargetingComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Component|TargetingComponent")
	TObjectPtr<UTargetingComponent>		CloneTechTargetingComponent = nullptr;

};
