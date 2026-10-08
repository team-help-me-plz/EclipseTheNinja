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
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual bool CheckCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Target")
	TSubclassOf<ACloneCharacter>	CloneClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|SpawnDist")
	float							CloneDist = 100.0f;
};
