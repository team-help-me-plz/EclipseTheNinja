#include "GAS/Ability/GABossPatternBase.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
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

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		PatternMontage,
		FMath::Max(0.01f, PlayRate)
	);
	MontageTask->OnCompleted.AddDynamic(this, &UGABossPatternBase::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UGABossPatternBase::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGABossPatternBase::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UGABossPatternBase::OnMontageInterrupted);
	MontageTask->ReadyForActivation();
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
