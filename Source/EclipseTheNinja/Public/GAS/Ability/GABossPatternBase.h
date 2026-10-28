#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GABossPatternBase.generated.h"

class UAnimMontage;
struct FBossAttackHitArea;

/**
 * 보스 공격 패턴 어빌리티의 공통 부모.
 * 패턴 하나 = 몽타주 1개 + 몽타주의 Boss Attack Hit 노티파이마다 OnPatternHit 호출.
 * 타격 시점에 무엇을 할지(근접 판정, 투사체 발사 등)는 파생 클래스가 정한다. 다단히트 패턴은 노티파이를 여러 개 둔다.
 * 시전 전 준비 구간(몽타주 앞부분)은 끝 자세에서 멈추거나 슬로우로 재생해 플레이어가 대비할 틈을 준다.
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
	// Boss Attack Hit 노티파이마다 그 노티파이의 판정 영역과 함께 호출된다. 파생 클래스가 판정이나 발사를 구현한다.
	virtual void OnPatternHit(const FBossAttackHitArea& HitArea);

	/** 패턴 몽타주. 보스 ABP의 DefaultSlot을 사용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern")
	TObjectPtr<UAnimMontage> PatternMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	/**
	 * 준비 구간을 늘이는 방식.
	 * true면 준비 구간 끝 위치에서 몽타주를 WindupExtraTime만큼 멈춰 자세를 잡는다.
	 * false면 준비 구간 전체를 WindupSlowRate 배속으로 느리게 재생한다.
	 * 어느 쪽이든 준비 구간이 끝나면 PlayRate로 시전한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Windup")
	bool bHoldWindupPose = true;

	/** 멈춤 방식에서 준비 구간 끝 자세로 멈춰 있는 시간(초). 플레이어가 패턴을 보고 대비할 틈을 준다. 0이면 멈추지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Windup", meta = (ClampMin = "0.0", Units = "s", EditCondition = "bHoldWindupPose"))
	float WindupExtraTime = 0.5f;

	/**
	 * 슬로우 방식에서 준비 구간의 재생 속도. PlayRate에 곱하는 배율이다.
	 * 0.5면 절반 속도로, 0.25면 4분의 1 속도로 재생한다. 1이면 늘이지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Windup", meta = (ClampMin = "0.05", ClampMax = "1.0", EditCondition = "!bHoldWindupPose"))
	float WindupSlowRate = 0.5f;

	/**
	 * 준비 구간이 끝나는 몽타주 위치(초, 몽타주 시간 기준). 칼을 휘두르기 직전 프레임을 고른다.
	 * 0이면 첫 Boss Attack Hit 노티파이 시점에 AutoWindupEndRatio를 곱한 위치를 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Windup", meta = (ClampMin = "0.0", Units = "s"))
	float WindupEndPosition = 0.0f;

	/** WindupEndPosition이 0일 때 첫 타격 노티파이 시점 중 앞에서 몇 비율까지를 준비 구간으로 볼지. 나머지는 휘두르는 동작으로 남긴다. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Windup", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AutoWindupEndRatio = 0.6f;

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

	// 멈춤 방식에서 몽타주가 준비 구간 끝에 닿으면 멈추고, WindupExtraTime 뒤에 다시 재생하도록 예약한다.
	UFUNCTION()
	void OnWindupHoldReached();

	// 늘인 준비 구간이 끝나면 원래 재생 속도로 되돌린다.
	UFUNCTION()
	void OnWindupFinished();

	// 준비 구간이 끝나는 몽타주 위치. WindupEndPosition이 0이면 첫 타격 노티파이로 정한다.
	float GetWindupEndPosition() const;
};
