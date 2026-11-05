#include "GAS/Ability/GASummonAllies.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Character/Enemy.h"
#include "Component/AllySummonComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Ability/GABaseAttack.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SummonAlliesEvent, "Event.Enemy.SummonAllies");

UGASummonAllies::UGASummonAllies()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	// 소리치는 중에는 기본 공격과 겹치지 않도록 같은 공격 중 태그를 사용한다.
	ActivationOwnedTags.AddTag(UGABaseAttack::GetAttackingTag());
	ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());
}

FGameplayTag UGASummonAllies::GetSummonEventTag()
{
	return TAG_SummonAlliesEvent;
}

bool UGASummonAllies::CanActivateAbility(
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

	const AEnemy* Enemy = ActorInfo
		? Cast<AEnemy>(ActorInfo->AvatarActor.Get())
		: nullptr;

	if (!Enemy || Enemy->IsDead() || !Enemy->GetMesh()->GetAnimInstance())
	{
		return false;
	}

	// 아직 부르지 않았고, 다른 소환 동작이 진행 중이 아닐 때만 시작한다.
	const UAllySummonComponent* SummonComp = Enemy->FindComponentByClass<UAllySummonComponent>();
	return SummonComp
		&& SummonComp->GetSummonMontage()
		&& SummonComp->WantsToSummon()
		&& !SummonComp->IsSummonInProgress();
}

void UGASummonAllies::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AEnemy* Enemy = Cast<AEnemy>(ActorInfo->AvatarActor.Get());
	UAllySummonComponent* SummonComp = GetSummonComponent();

	if (!Enemy || !SummonComp || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	SummonComp->SetSummonInProgress(true);

	// 제자리에 서서 대상을 바라보고 소리친다.
	Enemy->StopHorizontalMovement();

	if (const APawn* Target = SummonComp->GetSummonTarget())
	{
		Enemy->FaceLocation(Target->GetActorLocation());
	}

	// 몽타주 앞부분 노티파이를 놓치지 않도록 재생보다 먼저 건다.
	UAbilityTask_WaitGameplayEvent* SummonTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		GetSummonEventTag()
	);
	SummonTask->EventReceived.AddDynamic(this, &UGASummonAllies::OnSummonEvent);
	SummonTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		SummonComp->GetSummonMontage(),
		SummonComp->GetSummonPlayRate()
	);
	MontageTask->OnCompleted.AddDynamic(this, &UGASummonAllies::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UGASummonAllies::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGASummonAllies::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UGASummonAllies::OnMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UGASummonAllies::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	if (UAllySummonComponent* SummonComp = GetSummonComponent())
	{
		SummonComp->SetSummonInProgress(false);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGASummonAllies::OnSummonEvent(FGameplayEventData Payload)
{
	if (IsActive())
	{
		SummonNow();
	}
}

void UGASummonAllies::OnMontageCompleted()
{
	if (!IsActive())
	{
		return;
	}

	// 노티파이를 넣지 않은 몽타주라도 소환은 되도록 끝날 때 부른다.
	const UAllySummonComponent* SummonComp = GetSummonComponent();
	if (SummonComp && !SummonComp->HasSummoned())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 소환 몽타주에 Summon Allies 노티파이가 없어 끝날 때 소환합니다."),
			*GetNameSafe(GetAvatarActorFromActorInfo()));

		SummonNow();
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGASummonAllies::OnMontageInterrupted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
	}
}

UAllySummonComponent* UGASummonAllies::GetSummonComponent() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	return Avatar
		? Avatar->FindComponentByClass<UAllySummonComponent>()
		: nullptr;
}

void UGASummonAllies::SummonNow()
{
	UAllySummonComponent* SummonComp = GetSummonComponent();
	if (!SummonComp || SummonComp->HasSummoned())
	{
		return;
	}

	SummonComp->SummonAllies(SummonComp->GetSummonTarget());
}
