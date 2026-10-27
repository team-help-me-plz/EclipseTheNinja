

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GACooldownBase.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnAbilityCooldownStart);

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UGACooldownBase : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	void	GetCooldown(float& OutRemaining, float& OutDuration) const;

	FOnAbilityCooldownStart	OnAbilityCooldownStart;

protected:
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	mutable FActiveGameplayEffectHandle	CooldownEffectHandle;

};
