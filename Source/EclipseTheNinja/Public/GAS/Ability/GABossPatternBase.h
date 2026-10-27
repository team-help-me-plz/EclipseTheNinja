#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GABossPatternBase.generated.h"

class UAnimMontage;

/**
 * 보스 공격 패턴 어빌리티의 공통 부모.
 * 패턴 하나 = 몽타주 1개 + 타격 시점마다 OnPatternHit 호출. 타격 시점에 무엇을 할지(근접 판정, 투사체 발사 등)는 파생 클래스가 정한다.
 *
 * 타격 시점은 두 가지 방식을 함께 지원한다.
 * - 몽타주에 Basic Attack Hit 노티파이를 넣으면 노티파이 시점마다 호출한다.
 * - HitTimes에 몽타주 시작 기준 시간(초)을 넣으면 그 시점마다 호출한다. (노티파이를 넣기 전 테스트용)
 *
 * 보스는 다음 패턴을 고를 때 사용 거리 밖의 패턴을 후보에서 뺀다. 패턴 쿨타임은 DT_BossPatterns 행의 Cooldown으로 보스가 센다.
 */
UCLASS(Abstract)
class ECLIPSETHENINJA_API UGABossPatternBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGABossPatternBase();

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

	/** 이 패턴을 고를 수 있는 대상과의 최소 거리(cm). */
	float GetMinUseDistance() const { return MinUseDistance; }

	/** 이 패턴을 고를 수 있는 대상과의 최대 거리(cm). 0 이하면 보스의 AttackDistance를 사용한다. */
	float GetMaxUseDistance() const { return MaxUseDistance; }

protected:
	// 타격 시점(노티파이 또는 HitTimes)마다 호출된다. 파생 클래스가 판정이나 발사를 구현한다.
	virtual void OnPatternHit();

	/** 패턴 몽타주. 보스 ABP의 DefaultSlot을 사용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern")
	TObjectPtr<UAnimMontage> PatternMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	/** 몽타주 시작 기준 타격 시점(초, PlayRate 적용 전). 비워 두면 노티파이로만 처리한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern")
	TArray<float> HitTimes;

	/** 이 패턴을 고를 수 있는 대상과의 최소 거리(cm). 가까이 붙은 대상에게 원거리 패턴을 쓰지 않게 할 때 사용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Range", meta = (ClampMin = "0.0", Units = "cm"))
	float MinUseDistance = 0.0f;

	/** 이 패턴을 고를 수 있는 대상과의 최대 거리(cm). 0이면 보스의 AttackDistance를 사용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Range", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxUseDistance = 0.0f;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnHitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnHitTimeReached();
};
