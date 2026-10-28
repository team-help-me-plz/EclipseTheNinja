#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Containers/Ticker.h"
#include "GABossExecution.generated.h"

class ABossEnemy;
class APlayerCharacter;
class UAnimMontage;
class UBrainComponent;

/**
 * 에너지 링크 기믹에 실패했을 때 보스가 플레이어를 암살하는 연출.
 * 보스가 플레이어 등 뒤로 순간이동해 같은 방향을 보고, 보스는 암살자 몽타주를, 플레이어는 피격자 몽타주를 재생한다.
 * 플레이어는 피격자 몽타주를 시작할 때 사망 처리되고, 몽타주가 끝나면 기존 사망 흐름대로 체크포인트에서 리스폰한다.
 * 몽타주와 위치는 보스의 "Boss|Execution" 설정을 읽는다.
 * 칼이 박히는 순간의 게임 속도 연출은 플레이어 암살(UGAAssassinateBase)과 같은 규칙으로 동작한다.
 * 예비 동작 슬로모 → 임팩트 히트스톱 → 슬로모션 → 복귀 순이며, 기본값은 GA_Assassinate와 같다.
 */
UCLASS()
class ECLIPSETHENINJA_API UGABossExecution : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGABossExecution();

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
	// 보스를 플레이어 등 뒤로 옮기고 플레이어와 같은 방향을 보게 한다.
	void MoveBehindVictim(ABossEnemy* Boss, const APlayerCharacter* Player) const;

	// 두 캐릭터의 캡슐이 서로 밀어내지 않게 한다.
	void SetIgnoreVictimCollision(bool bIgnore);

	UFUNCTION()
	void OnExecutionFinished();

	// 칼이 박히는 순간을 기다렸다가 히트스톱·슬로모션을 건다. 암살자 몽타주를 재생하기 직전에 호출한다.
	// 암살자 몽타주에 AnimNotify_AssassinateImpact가 있으면 그 프레임을, 없으면 ImpactFallbackTime을 쓴다.
	void WaitForImpact();

	/** 임팩트 순간에 게임 속도를 늦출지 여부 */
	UPROPERTY(EditDefaultsOnly, Category = "Execution|Impact")
	bool bUseImpactTimeDilation = true;

	/** 몽타주에 Assassinate Impact 노티파이가 없을 때 쓰는 임팩트 시점(몽타주 시작 기준). 음수면 노티파이만 기다린다. */
	UPROPERTY(EditDefaultsOnly, Category = "Execution|Impact", meta = (EditCondition = "bUseImpactTimeDilation", Units = "s"))
	float ImpactFallbackTime = -1.0f;

	/** 히트스톱 동안의 게임 속도 배율 */
	UPROPERTY(EditDefaultsOnly, Category = "Execution|Impact", meta = (EditCondition = "bUseImpactTimeDilation", ClampMin = "0.001", ClampMax = "1.0"))
	float HitStopTimeScale = 0.1f;

	/** 히트스톱 길이(실제 시간) */
	UPROPERTY(EditDefaultsOnly, Category = "Execution|Impact", meta = (EditCondition = "bUseImpactTimeDilation", ClampMin = "0.0", Units = "s"))
	float HitStopDuration = 0.75f;

	/** 히트스톱 뒤 슬로모션 동안의 게임 속도 배율 */
	UPROPERTY(EditDefaultsOnly, Category = "Execution|Impact", meta = (EditCondition = "bUseImpactTimeDilation", ClampMin = "0.001", ClampMax = "1.0"))
	float SlowMotionTimeScale = 0.5f;

	/** 슬로모션 길이(실제 시간) */
	UPROPERTY(EditDefaultsOnly, Category = "Execution|Impact", meta = (EditCondition = "bUseImpactTimeDilation", ClampMin = "0.0", Units = "s"))
	float SlowMotionDuration = 1.0f;

	/** 슬로모션에서 원래 속도로 돌아오는 시간(실제 시간). 시작과 끝을 부드럽게 이어 속도가 튀지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Execution|Impact", meta = (EditCondition = "bUseImpactTimeDilation", ClampMin = "0.0", Units = "s"))
	float RecoverDuration = 0.12f;

	/** 칼을 뻗기 전 예비 동작을 느리게 보여 줄지 여부. 칼이 들어가는 구간은 원래 속도로 돌아온다. */
	UPROPERTY(EditDefaultsOnly, Category = "Execution|Impact", meta = (EditCondition = "bUseImpactTimeDilation"))
	bool bUseAnticipation = true;

	/** 예비 동작 동안의 게임 속도 배율 */
	UPROPERTY(EditDefaultsOnly, Category = "Execution|Impact", meta = (EditCondition = "bUseImpactTimeDilation && bUseAnticipation", ClampMin = "0.05", ClampMax = "1.0"))
	float AnticipationTimeScale = 0.25f;

	/** 예비 동작 배율까지 서서히 느려지는 시간(실제 시간). 한 번에 바꾸면 끊겨 보인다. */
	UPROPERTY(EditDefaultsOnly, Category = "Execution|Impact", meta = (EditCondition = "bUseImpactTimeDilation && bUseAnticipation", ClampMin = "0.0", Units = "s"))
	float AnticipationBlendTime = 0.1f;

	/**
	 * 임팩트보다 얼마나 앞(몽타주 시간)에서 예비 동작 슬로모를 시작할지.
	 * 플레이어 암살과 같이, 이 값이 임팩트 시점보다 길면 예비 동작 슬로모를 걸지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Execution|Impact", meta = (EditCondition = "bUseImpactTimeDilation && bUseAnticipation", ClampMin = "0.0", Units = "s"))
	float AnticipationLeadTime = 1.0f;

	/** 임팩트보다 얼마나 앞(몽타주 시간)에서 원래 속도로 되돌려 칼을 빠르게 꽂을지 */
	UPROPERTY(EditDefaultsOnly, Category = "Execution|Impact", meta = (EditCondition = "bUseImpactTimeDilation && bUseAnticipation", ClampMin = "0.0", Units = "s"))
	float SnapLeadTime = 0.06f;

	// 이번 처형에서 보스가 재생하는 암살자 몽타주
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ExecutionAttackMontage;

	TWeakObjectPtr<APlayerCharacter> Victim;
	TWeakObjectPtr<UBrainComponent> PausedBrain;
	bool bIgnoringVictimCollision = false;

private:
	UFUNCTION()
	void OnImpactEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnImpactFallback();

	UFUNCTION()
	void OnAnticipationStart();

	UFUNCTION()
	void OnAnticipationSnap();

	// 암살자 몽타주에서 Assassinate Impact 노티파이가 울리는 몽타주 시간을 찾는다.
	bool FindImpactNotifyTime(float& OutTime) const;

	// 처음 바꿀 때의 전역 배율을 기준으로 게임 속도를 Scale배로 맞춘다.
	void SetTimeScale(float Scale);

	void StartImpactTimeDilation();

	// 게임 속도 배율과 무관하게 실제 시간으로 진행해야 하므로 코어 티커로 돈다.
	bool TickImpactTimeDilation(float DeltaTime);

	void StartTimeDilationTicker();

	void StopTimeDilationTicker();

	// 게임 속도를 원래대로 되돌린다. 티커 콜백 안에서 부를 때는 bRemoveTicker를 false로 넘긴다.
	void StopImpactTimeDilation(bool bRemoveTicker);

	enum class ETimeDilationPhase : uint8
	{
		None,
		Anticipation,
		Impact
	};

	FTSTicker::FDelegateHandle ImpactTickerHandle;
	ETimeDilationPhase TimeDilationPhase = ETimeDilationPhase::None;
	float ImpactElapsed = 0.0f;
	float SavedTimeDilation = 1.0f;
	bool bImpactTriggered = false;
	bool bTimeDilationApplied = false;
};
