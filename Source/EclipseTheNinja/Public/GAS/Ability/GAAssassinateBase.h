#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Containers/Ticker.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/Ability/AbilityAssetPreloader.h"
#include "Component/AssassinationTargetingComponent.h"
#include "GAAssassinateBase.generated.h"

class APlayerCharacter;
class AEnemy;
class UAssassinateTargetComponent;
class UBrainComponent;
class UAnimMontage;
class UCurveVector;
class UNiagaraSystem;
class UAbilityTask_ApplyRootMotionMoveToForce;

/**
 * 평지·공중·아래 암살이 공유하는 흐름.
 * 대상 예약, 적 AI·이동 정지, 실행 몽타주와 처치, 종료 시 복구를 처리하고
 * 대상 조건과 접근 방식만 자식 클래스가 정한다.
 */
UCLASS(Abstract)
class ECLIPSETHENINJA_API UGAAssassinateBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGAAssassinateBase();
	static FGameplayTag GetAssassinatingTag();
	static FGameplayTag GetImpactEventTag();
	static FGameplayTag GetVFXEventTag();

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
	// 어빌리티를 받을 때 암살·피격 몽타주와 임팩트 VFX를 미리 불러 둔다. 첫 암살 때 로딩으로 멈추지 않게 한다.
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

protected:
	// 공통 조건을 통과한 대상에 대해 이 암살 방식으로 실행할 수 있는지 확인한다.
	virtual bool CanAssassinateTarget(const APlayerCharacter* Player, const UAssassinateTargetComponent* Target,
		const AEnemy* Enemy) const PURE_VIRTUAL(UGAAssassinateBase::CanAssassinateTarget, return false;);

	// 이 어빌리티의 암살 방식. 보스가 살아남을 때 방식에 맞는 리액션을 고르는 데 쓴다.
	virtual EAssassinationType GetAssassinationType() const { return EAssassinationType::Ground; }

	// 공통 준비가 끝난 뒤 실행 위치까지 접근한다. 도착하면 PlayExecution을 호출한다.
	virtual void StartApproach() PURE_VIRTUAL(UGAAssassinateBase::StartApproach, );

	// 실행 몽타주를 재생하고 피격자 몽타주로 처치를 확정한다.
	void PlayExecution();

	// 암살자 몽타주만 재생한다. 끝나면 어빌리티를 종료한다. 실패하면 false.
	bool PlayAttackMontage();

	// 암살자 몽타주를 StartTime부터 다시 재생한다. 엔진이 이전 재생과 몽타주의 Blend In 시간만큼 섞어 주므로,
	// 몽타주 안의 두 시퀀스 경계처럼 자세가 크게 바뀌는 곳을 부드럽게 넘길 때 쓴다.
	bool RestartAttackMontageAt(float StartTime);

	// 피격자 몽타주로 기존 사망 처리를 실행해 처치를 확정한다.
	void ExecuteVictim();

	/**
	 * 피격자를 옮겨 TimeToContact 뒤(칼이나 손이 몸에 닿는 순간)에 도착하게 한다.
	 * 페어 애니메이션은 두 캐릭터가 정해진 간격에서 맞물리도록 만들어져 있어서, 암살자는 평면에 두고
	 * 피격자만 그 차이만큼 옮긴다. 서 있는 동안 미끄러지면 눈에 띄므로 닿기 직전 VictimSlideTime 동안만 움직인다.
	 * @param SideOffset    암살자 기준 오른쪽이 +, 왼쪽이 -(cm). 횡스크롤에서는 화면 깊이 방향(월드 Y축)이다.
	 * @param ForwardOffset 피격자가 바라보는 방향으로 밀 거리(cm)
	 */
	void SlideVictim(float SideOffset, float ForwardOffset, float TimeToContact);

	UFUNCTION()
	void StartVictimSlide();

	// 살아남은 피격자(보스)를 옆으로 옮긴 만큼 되돌려 원래 이동 평면에 세운다.
	UFUNCTION()
	void ReturnSlidVictim();

	/** 피격자를 옆으로 옮기는 시간. 칼이나 손이 닿기 직전 이 시간 동안만 움직인다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination", meta = (ClampMin = "0.05", Units = "s"))
	float VictimSlideTime = 0.3f;

	// 암살자 몽타주의 루트 모션을 켜거나 끈다. 이동을 코드가 맡는 구간에서는 끈다.
	void SetAttackRootMotionEnabled(bool bEnabled);

	// 코드가 맡던 이동을 몽타주 루트 모션에 넘긴다. 걷기 상태로 바꾸고, 발판 끝에서 떨어지지 않게 막는다.
	// 종료 시 발판 끝 설정은 자동으로 되돌린다.
	void HandOverToAttackRootMotion();

	// 암살자가 이동하는 동안 대상 캡슐에 막히지 않게 한다. 종료 시 자동으로 되돌린다.
	void SetIgnoreVictimCollision(bool bIgnore);

	// 암살자 몽타주 루트 모션이 [StartTime, StartTime + Duration] 구간에서 진행한 비율을 따라 End까지 이동한다.
	// 실제 거리·높이에 맞춰 늘리거나 줄이므로 발판 높이가 애니메이션과 달라도 움직임의 모양은 유지된다.
	UAbilityTask_ApplyRootMotionMoveToForce* MoveAlongAttackRootMotion(float StartTime, float Duration, const FVector& End);

	// 히트스톱을 예약한다. 암살자 몽타주를 재생하기 직전에 호출한다.
	// 히트스톱은 AnimNotify_AssassinateImpact가 있으면 그 프레임을, 없으면 ImpactFallbackTime을 쓴다.
	// 몽타주를 중간부터 다시 재생할 때는 그 시작 위치를 CurrentMontageTime으로 넘긴다.
	void WaitForImpact(float CurrentMontageTime = 0.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Assassination|VFX")
	bool bUseImpactVFX = true;

	UPROPERTY(EditDefaultsOnly, Category = "Assassination|VFX", meta = (EditCondition = "bUseImpactVFX"))
	TSoftObjectPtr<UNiagaraSystem> ImpactVFX;

	/** 피격자 메시의 소켓/본. 없으면 피격자 액터 위치를 사용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|VFX", meta = (EditCondition = "bUseImpactVFX"))
	FName ImpactVFXSocket = TEXT("spine_03");

	/** 피격자 소켓 기준 위치·회전 보정. 생성 후에는 월드에 남아 데칼이 함께 움직이지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|VFX", meta = (EditCondition = "bUseImpactVFX"))
	FTransform ImpactVFXOffset = FTransform::Identity;

	/** 에셋의 User.LifeTimeMult 값(수명 배율). */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|VFX", meta = (EditCondition = "bUseImpactVFX", ClampMin = "0.01"))
	float ImpactVFXLifeTime = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Assassination|VFX", meta = (EditCondition = "bUseImpactVFX", ClampMin = "0.01"))
	float ImpactVFXScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Assassination|VFX", meta = (EditCondition = "bUseImpactVFX", ClampMin = "0.01"))
	float ImpactVFXTextureScale = 1.2f;

	/** 임팩트 순간에 게임 속도를 늦출지 여부 */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Impact")
	bool bUseImpactTimeDilation = true;

	/** 몽타주에 Assassinate Impact 노티파이가 없을 때 쓰는 임팩트 시점(몽타주 시작 기준). 음수면 노티파이만 기다린다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Impact", meta = (Units = "s"))
	float ImpactFallbackTime = 0.4f;

	/** 아래 암살 소리를 임팩트보다 얼마나 늦게 낼지(실제 시간). 히트스톱·슬로모에 늘어나지 않는다. 아래 암살에서만 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Sound", meta = (ClampMin = "0.0", Units = "s"))
	float BelowAssassinationSoundDelay = 0.1f;

	/** 히트스톱 동안의 게임 속도 배율 */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Impact", meta = (EditCondition = "bUseImpactTimeDilation", ClampMin = "0.001", ClampMax = "1.0"))
	float HitStopTimeScale = 0.02f;

	/** 히트스톱 길이(실제 시간) */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Impact", meta = (EditCondition = "bUseImpactTimeDilation", ClampMin = "0.0", Units = "s"))
	float HitStopDuration = 0.1f;

	/** 히트스톱 뒤 슬로모션 동안의 게임 속도 배율 */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Impact", meta = (EditCondition = "bUseImpactTimeDilation", ClampMin = "0.001", ClampMax = "1.0"))
	float SlowMotionTimeScale = 0.2f;

	/** 슬로모션 길이(실제 시간) */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Impact", meta = (EditCondition = "bUseImpactTimeDilation", ClampMin = "0.0", Units = "s"))
	float SlowMotionDuration = 0.12f;

	/** 슬로모션에서 원래 속도로 돌아오는 시간(실제 시간). 시작과 끝을 부드럽게 이어 속도가 튀지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Impact", meta = (EditCondition = "bUseImpactTimeDilation", ClampMin = "0.0", Units = "s"))
	float RecoverDuration = 0.4f;

	/** 칼을 뻗기 전 예비 동작을 느리게 보여 줄지 여부. 칼이 들어가는 구간은 원래 속도로 돌아온다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Impact", meta = (EditCondition = "bUseImpactTimeDilation"))
	bool bUseAnticipation = true;

	/** 예비 동작 동안의 게임 속도 배율 */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Impact", meta = (EditCondition = "bUseImpactTimeDilation && bUseAnticipation", ClampMin = "0.05", ClampMax = "1.0"))
	float AnticipationTimeScale = 0.45f;

	/** 예비 동작 배율까지 서서히 느려지는 시간(실제 시간). 한 번에 바꾸면 끊겨 보인다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Impact", meta = (EditCondition = "bUseImpactTimeDilation && bUseAnticipation", ClampMin = "0.0", Units = "s"))
	float AnticipationBlendTime = 0.1f;

	/** 임팩트보다 얼마나 앞(몽타주 시간)에서 예비 동작 슬로모를 시작할지 */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Impact", meta = (EditCondition = "bUseImpactTimeDilation && bUseAnticipation", ClampMin = "0.0", Units = "s"))
	float AnticipationLeadTime = 0.25f;

	/** 임팩트보다 얼마나 앞(몽타주 시간)에서 원래 속도로 되돌려 칼을 빠르게 꽂을지 */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Impact", meta = (EditCondition = "bUseImpactTimeDilation && bUseAnticipation", ClampMin = "0.0", Units = "s"))
	float SnapLeadTime = 0.07f;

	UFUNCTION()
	void OnCompleted();

	UFUNCTION()
	void OnInterrupted();

	UFUNCTION()
	void OnVictimDestroyed(AActor* DestroyedActor);

	// 암살자 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "Assassination")
	TSoftObjectPtr<UAnimMontage> AttackMontage;

	// 암살 당하는 적의 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "Assassination")
	TSoftObjectPtr<UAnimMontage> VictimMontage;

	TWeakObjectPtr<APlayerCharacter> Assassin;
	TWeakObjectPtr<AEnemy> Victim;
	TWeakObjectPtr<UAssassinateTargetComponent> TargetComponent;
	TWeakObjectPtr<UBrainComponent> PausedBrain;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ExecutionMontage;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> VictimExecutionMontage;

	// MoveAlongAttackRootMotion이 이동 태스크에 넘기는 경로 곡선(X: 진행 방향, Z: 위).
	UPROPERTY(Transient)
	TObjectPtr<UCurveVector> RootMotionPathCurve;

	FRotator ExecutionRotation = FRotator::ZeroRotator;
	bool bAttackRootMotionDisabled = false;
	bool bIgnoringVictimCollision = false;
	bool bRestartingAttackMontage = false;
	float PendingVictimSideOffset = 0.0f;
	float PendingVictimForwardOffset = 0.0f;
	float PendingVictimSlideDuration = 0.0f;
	// 피격자를 옮긴 양. 살아남으면 암살이 끝나기 전에 이만큼 되돌린다.
	FVector VictimSlideDelta = FVector::ZeroVector;
	FVector VictimSlideOrigin = FVector::ZeroVector;
	// 옮긴 뒤 아직 원래 평면으로 완전히 돌아오지 않았으면 true. 종료 시 남은 만큼 되돌린다.
	bool bVictimSlidePending = false;
	bool bLedgeWalkLocked = false;
	bool bSavedCanWalkOffLedges = true;

private:
	bool StartAttackMontageTask(float StartTime);
	void TriggerImpact();

	UFUNCTION()
	void PlayAssassinationStabSound();
	// 아래 암살 소리를 BelowAssassinationSoundDelay만큼 실제 시간으로 늦춰 낸다.
	void PlayBelowAssassinationSound();
	void PlayImpactVFX();
	UFUNCTION()
	void OnVFXEvent(FGameplayEventData Payload);

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedImpactVFX;

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
	void StopImpactTimeDilation(bool bRemoveTicker);

	enum class ETimeDilationPhase : uint8 { None, Anticipation, Impact };

	FTSTicker::FDelegateHandle ImpactTickerHandle;
	ETimeDilationPhase TimeDilationPhase = ETimeDilationPhase::None;
	float ImpactElapsed = 0.0f;
	float SavedTimeDilation = 1.0f;
	bool bImpactTriggered = false;
	bool bPlayBossImpactSound = false;
	// 피격자에게 암살 피해를 적용했는지. 살아남은 대상을 종료 시 전투 상태로 바꿀 때 쓴다.
	bool bVictimExecuted = false;
	bool bImpactVFXTriggered = false;
	bool bTimeDilationApplied = false;

protected:
	TEnumAsByte<EMovementMode> PlayerMovementMode = MOVE_Walking;
	TEnumAsByte<EMovementMode> VictimMovementMode = MOVE_Walking;
	uint8 PlayerCustomMode = 0;
	uint8 VictimCustomMode = 0;
	bool bMovementLocked = false;
	bool bTargetReserved = false;
	bool bEnding = false;

private:
	// 미리 불러 둔 몽타주·VFX를 메모리에 잡아 둔다. 어빌리티를 잃으면 놓는다.
	FAbilityAssetPreloader AssetPreloader;
};
