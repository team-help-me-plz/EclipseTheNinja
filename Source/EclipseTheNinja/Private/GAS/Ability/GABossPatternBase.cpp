#include "GAS/Ability/GABossPatternBase.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Animation/AnimNotify_BossAttackHit.h"

UGABossPatternBase::UGABossPatternBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	// 기본 공격과 같은 공격 중 태그를 사용해 패턴끼리 겹쳐 실행되지 않게 한다.
	ActivationOwnedTags.AddTag(UGABaseAttack::GetAttackingTag());
	ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());
}

bool UGABossPatternBase::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags
) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 몽타주와 재생할 애님 인스턴스가 있어야 한다.
	const AEnemy* Enemy = ActorInfo ? Cast<AEnemy>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Enemy || !PatternMontage)
	{
		return false;
	}

	if (!Enemy->GetMesh()->GetAnimInstance())
	{
		return false;
	}

	// 살아 있고, 공격 상태로 전환되었고, 공격할 대상이 있어야 한다.
	if (Enemy->IsDead() || Enemy->GetBehaviorState() != EEnemyBehaviorState::Attack)
	{
		return false;
	}

	return Enemy->GetCombatTarget() != nullptr;
}

void UGABossPatternBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	// 몽타주 앞부분에 노티파이가 있을 수 있으므로 재생보다 먼저 이벤트 대기를 건다.
	UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		UGABaseAttack::GetHitEventTag()
	);
	HitTask->EventReceived.AddDynamic(this, &UGABossPatternBase::OnHitEvent);
	HitTask->ReadyForActivation();

	// 준비 구간(몽타주 시작~준비 끝 위치)은 끝에서 멈추거나 느리게 재생해 늘이고, 그 뒤부터 원래 속도로 시전한다.
	const float BasePlayRate = FMath::Max(0.01f, PlayRate);
	const float WindupEnd = GetWindupEndPosition();
	const float NormalWindupDuration = WindupEnd / BasePlayRate;
	const float SlowRate = FMath::Clamp(WindupSlowRate, 0.05f, 1.0f);
	const bool bHoldWindup = bHoldWindupPose && WindupExtraTime > 0.0f;
	const bool bStretchWindup = !bHoldWindupPose && WindupEnd > 0.0f && SlowRate < 1.0f;
	const float StartPlayRate = bStretchWindup ? BasePlayRate * SlowRate : BasePlayRate;
	const float StretchedWindupDuration = NormalWindupDuration / SlowRate;

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		PatternMontage,
		StartPlayRate
	);
	MontageTask->OnCompleted.AddDynamic(this, &UGABossPatternBase::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UGABossPatternBase::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGABossPatternBase::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UGABossPatternBase::OnMontageInterrupted);
	MontageTask->ReadyForActivation();

	if (bHoldWindup)
	{
		UAbilityTask_WaitDelay* HoldTask = UAbilityTask_WaitDelay::WaitDelay(
			this,
			NormalWindupDuration
		);
		HoldTask->OnFinish.AddDynamic(this, &UGABossPatternBase::OnWindupHoldReached);
		HoldTask->ReadyForActivation();
	}
	else if (bStretchWindup)
	{
		UAbilityTask_WaitDelay* WindupTask = UAbilityTask_WaitDelay::WaitDelay(
			this,
			StretchedWindupDuration
		);
		WindupTask->OnFinish.AddDynamic(this, &UGABossPatternBase::OnWindupFinished);
		WindupTask->ReadyForActivation();
	}
}

void UGABossPatternBase::OnWindupHoldReached()
{
	const AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	UAnimInstance* AnimInstance = Enemy ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	if (!IsActive() || !AnimInstance || !AnimInstance->Montage_IsPlaying(PatternMontage))
	{
		return;
	}

	// 대기 타이머와 실제 재생 위치가 프레임 단위로 어긋날 수 있어 준비 끝 위치로 맞춘 뒤 멈춘다.
	AnimInstance->Montage_SetPosition(PatternMontage, GetWindupEndPosition());
	AnimInstance->Montage_Pause(PatternMontage);

	UAbilityTask_WaitDelay* WindupTask = UAbilityTask_WaitDelay::WaitDelay(
		this,
		WindupExtraTime
	);
	WindupTask->OnFinish.AddDynamic(this, &UGABossPatternBase::OnWindupFinished);
	WindupTask->ReadyForActivation();
}

float UGABossPatternBase::GetWindupEndPosition() const
{
	if (!PatternMontage)
	{
		return 0.0f;
	}

	const float MontageLength = PatternMontage->GetPlayLength();
	if (WindupEndPosition > 0.0f)
	{
		return FMath::Min(WindupEndPosition, MontageLength);
	}

	// 따로 정하지 않으면 첫 타격 노티파이 앞부분의 AutoWindupEndRatio만큼을 준비 구간으로 본다.
	float FirstHitTime = MontageLength;
	for (const FAnimNotifyEvent& NotifyEvent : PatternMontage->Notifies)
	{
		if (Cast<UAnimNotify_BossAttackHit>(NotifyEvent.Notify))
		{
			FirstHitTime = FMath::Min(FirstHitTime, NotifyEvent.GetTriggerTime());
		}
	}

	return FirstHitTime * AutoWindupEndRatio;
}

void UGABossPatternBase::OnWindupFinished()
{
	const AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	UAnimInstance* AnimInstance = Enemy ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	if (!IsActive() || !AnimInstance)
	{
		return;
	}

	// 멈춤 방식이면 다시 재생하고, 늘이기 방식이면 원래 속도로 되돌린다.
	AnimInstance->Montage_Resume(PatternMontage);
	AnimInstance->Montage_SetPlayRate(PatternMontage, FMath::Max(0.01f, PlayRate));
}

void UGABossPatternBase::OnPatternHit(const FBossAttackHitArea& HitArea)
{
}

void UGABossPatternBase::OnMontageCompleted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
	}
}

void UGABossPatternBase::OnMontageInterrupted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
	}
}

void UGABossPatternBase::OnHitEvent(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}

	// Boss Attack Hit 노티파이는 자신을 OptionalObject로 보내 판정 영역을 함께 전달한다.
	// 같은 이벤트 태그를 쓰는 Basic Attack Hit 노티파이는 보스 패턴에서 쓰지 않으므로 무시한다.
	const UAnimNotify_BossAttackHit* HitNotify = Cast<UAnimNotify_BossAttackHit>(Payload.OptionalObject.Get());
	if (!HitNotify)
	{
		return;
	}

	OnPatternHit(HitNotify->GetHitArea());
}
