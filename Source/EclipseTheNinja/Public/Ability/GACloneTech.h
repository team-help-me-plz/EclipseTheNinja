// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/GATargetingAbility.h"
#include "GACloneTech.generated.h"

class ACloneCharacter;

USTRUCT(BlueprintType)
struct FTargetingResult
{
	GENERATED_BODY()

};
/**
 * 
 */

UCLASS()
class ECLIPSETHENINJA_API UGACloneTech : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGACloneTech();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual bool CheckCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void InputPressed(
		const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	UFUNCTION()
	void	HandleCloneDestroyed(AActor* DestroyedActor);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Target")
	TSubclassOf<ACloneCharacter>	CloneClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|SpawnDist")
	float							CloneDist = 100.0f;

	UPROPERTY()
	TWeakObjectPtr<ACloneCharacter>	SpawnedClone = nullptr;
private:
	bool	bIsSwaped = false;

};
