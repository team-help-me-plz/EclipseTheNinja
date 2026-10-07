// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "GameplayAbilitySpecHandle.h"
#include "GameplayEffectTypes.h"
#include "AbilitySystemInterface.h"
#include "Variant_SideScrolling/SideScrollingCharacter.h"
#include "CloneTechTestCharacter.generated.h"

class UAbilitySystemComponent;
class UInputAction;

USTRUCT(BlueprintType)
struct FAbilityInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 AbilityLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 InputID = INDEX_NONE;
};

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API ACloneTechTestCharacter : public ASideScrollingCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ACloneTechTestCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void	GiveInitAbility();

	void	OnLocalInputStart(int32 InID);
	void	OnLocalInputCompleted(int32 InID);

	void			DoCloneTech();
	virtual void	DoInteract() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	TArray<FAbilityInfo>		AbilityClassArray;

	//UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	//TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction>	CloneTechAction;

	TObjectPtr<AActor>			CloneTarget = nullptr;

private:
	TMap<int32, FGameplayAbilitySpecHandle>	AbilityHandles;

	//static constexpr int32 InteractInputID = 100;
	static constexpr int32 CloneTechInputID = 100;

};
