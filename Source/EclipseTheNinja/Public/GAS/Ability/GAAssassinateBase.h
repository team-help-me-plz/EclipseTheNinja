#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAAssassinateBase.generated.h"

class APlayerCharacter;
class AEnemy;
class UAssassinateTargetComponent;
class UBrainComponent;
class UAnimMontage;
class UCurveVector;
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
	// 공통 조건을 통과한 대상에 대해 이 암살 방식으로 실행할 수 있는지 확인한다.
	virtual bool CanAssassinateTarget(const APlayerCharacter* Player, const UAssassinateTargetComponent* Target,
		const AEnemy* Enemy) const PURE_VIRTUAL(UGAAssassinateBase::CanAssassinateTarget, return false;);

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

	// 암살자 몽타주의 루트 모션을 켜거나 끈다. 이동을 코드가 맡는 구간에서는 끈다.
	void SetAttackRootMotionEnabled(bool bEnabled);

	// 암살자가 이동하는 동안 대상 캡슐에 막히지 않게 한다. 종료 시 자동으로 되돌린다.
	void SetIgnoreVictimCollision(bool bIgnore);

	// 암살자 몽타주 루트 모션이 [StartTime, StartTime + Duration] 구간에서 진행한 비율을 따라 End까지 이동한다.
	// 실제 거리·높이에 맞춰 늘리거나 줄이므로 발판 높이가 애니메이션과 달라도 움직임의 모양은 유지된다.
	UAbilityTask_ApplyRootMotionMoveToForce* MoveAlongAttackRootMotion(float StartTime, float Duration, const FVector& End);

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

private:
	bool StartAttackMontageTask(float StartTime);

protected:
	TEnumAsByte<EMovementMode> PlayerMovementMode = MOVE_Walking;
	TEnumAsByte<EMovementMode> VictimMovementMode = MOVE_Walking;
	uint8 PlayerCustomMode = 0;
	uint8 VictimCustomMode = 0;
	bool bMovementLocked = false;
	bool bTargetReserved = false;
	bool bEnding = false;
};
