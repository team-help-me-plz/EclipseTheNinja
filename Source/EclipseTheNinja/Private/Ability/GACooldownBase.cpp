


#include "Ability/GACooldownBase.h"

void UGACooldownBase::GetCooldown(float& OutRemaining, float& OutDuration) const
{
	GetCooldownTimeRemainingAndDuration(
		GetCurrentAbilitySpecHandle(),
		GetCurrentActorInfo(),
		OutRemaining,
		OutDuration
	);
}

void UGACooldownBase::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	this->CooldownEffectHandle = FActiveGameplayEffectHandle();

	UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (CooldownGE)
	{
		this->CooldownEffectHandle = ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, CooldownGE, GetAbilityLevel(Handle, ActorInfo));
		UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
		OnAbilityCooldownStart.Broadcast();
	}
}
