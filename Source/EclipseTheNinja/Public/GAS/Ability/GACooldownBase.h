

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

	// 어빌리티를 쓰지 않고 같은 효과를 낸 경우(예: 컷신이 대신 분신을 소환)에 쿨타임만 시작한다
	void	StartCooldownWithoutActivation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const;

	FOnAbilityCooldownStart	OnAbilityCooldownStart;

protected:
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	mutable FActiveGameplayEffectHandle	CooldownEffectHandle;

};
