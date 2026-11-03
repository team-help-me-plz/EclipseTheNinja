#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GASummonAllies.generated.h"

class UAllySummonComponent;

/**
 * 소리쳐 동료를 부르는 어빌리티. UAllySummonComponent를 가진 적이 쓴다.
 *
 * 컴포넌트의 SummonMontage를 재생하고, 몽타주의 "Summon Allies" 노티파이 이벤트를 받으면 동료를 부른다.
 * 노티파이 없이 몽타주가 끝나면 경고를 남기고 그때 부른다. 부르기 전에 맞아서 끊기면 부르지 않고,
 * 컴포넌트가 계속 소환을 원하므로 BT가 다시 시도한다.
 * 소환 중에는 기본 공격과 겹치지 않도록 공격 중 태그를 쓴다.
 */
UCLASS()
class ECLIPSETHENINJA_API UGASummonAllies : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGASummonAllies();

	/** 몽타주 노티파이가 보내는 소환 이벤트 태그 */
	static FGameplayTag GetSummonEventTag();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr
	) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

protected:
	UFUNCTION()
	void OnSummonEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

private:
	// 아바타의 소환 컴포넌트
	UAllySummonComponent* GetSummonComponent() const;

	// 아직 부르지 않았으면 지금 대상으로 동료를 부른다.
	void SummonNow();
};
