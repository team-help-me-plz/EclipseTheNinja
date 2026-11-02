#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GACeilingAmbush.generated.h"

/**
 * 천장 매복 적이 떨어지며 아래 대상을 찌르는 기습 공격.
 *
 * 몽타주는 ACeilingEnemy::GetAmbushMontage가 준비한다. 타격은 기본 공격과 같은 Hit 이벤트로 받으며,
 * 적은 착지 순간 이 이벤트를 보낸다. 몽타주에 "Basic Attack Hit" 노티파이를 넣어도 같은 판정이 실행되고,
 * 한 번 맞은 대상은 이번 기습에서 다시 맞지 않는다.
 * 피해 적용은 UGABaseAttack과 같이 UGEBasicAttackDamage에 SetByCaller "Damage"로 공격력을 넘긴다.
 */
UCLASS()
class ECLIPSETHENINJA_API UGACeilingAmbush : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGACeilingAmbush();

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
	/** 착지 지점(캡슐 바닥) 기준 타격 판정 구의 반지름(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Ambush", meta = (ClampMin = "1.0", Units = "cm"))
	float HitRadius = 150.0f;

	/** 공격력에 곱할 배율. 1이면 기본 공격과 같은 피해다. */
	UPROPERTY(EditDefaultsOnly, Category = "Ambush", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.5f;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	// 착지 또는 노티파이에서 보낸 Hit 이벤트를 받아 주변 대상에게 피해를 준다.
	UFUNCTION()
	void OnHitEvent(FGameplayEventData Payload);

private:
	// 이번 기습에서 이미 피해를 준 대상
	TSet<TWeakObjectPtr<AActor>> HitActors;
};
