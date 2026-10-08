

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GABaseAttack.generated.h"

/** One click, one montage, and at most one damage application per target. */
UCLASS()
class ECLIPSETHENINJA_API UGABaseAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGABaseAttack();
	static FGameplayTag GetHitEventTag();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Attack", meta = (ClampMin = "1.0"))
	float AttackRange = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Attack", meta = (ClampMin = "1.0"))
	float AttackRadius = 50.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Attack", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	UFUNCTION()
	void OnAttackCompleted();

	UFUNCTION()
	void OnAttackInterrupted();

	UFUNCTION()
	void OnHitEvent(FGameplayEventData Payload);

private:
	TSet<TWeakObjectPtr<AActor>> HitActors;
};
