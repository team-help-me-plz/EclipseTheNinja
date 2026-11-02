#include "GAS/Ability/GACeilingAmbush.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/CeilingEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Effects/GEBasicAttackDamage.h"
#include "GAS/StatAttributeSet.h"

UGACeilingAmbush::UGACeilingAmbush()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	// 기습 중에는 기본 공격과 겹치지 않도록 같은 공격 중 태그를 사용한다.
	ActivationOwnedTags.AddTag(UGABaseAttack::GetAttackingTag());
	ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());
}

bool UGACeilingAmbush::CanActivateAbility(
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

	// 기습 단계에 들어간 천장 적만 사용할 수 있다.
	const ACeilingEnemy* Enemy = ActorInfo
		? Cast<ACeilingEnemy>(ActorInfo->AvatarActor.Get())
		: nullptr;

	if (!Enemy || Enemy->IsDead())
	{
		return false;
	}

	if (Enemy->GetCeilingPhase() != ECeilingEnemyPhase::Ambushing)
	{
		return false;
	}

	return Enemy->GetMesh()->GetAnimInstance() != nullptr;
}

void UGACeilingAmbush::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACeilingEnemy* Enemy = Cast<ACeilingEnemy>(ActorInfo->AvatarActor.Get());
	UAnimMontage* AmbushMontage = Enemy ? Enemy->GetAmbushMontage() : nullptr;

	// 몽타주를 준비하지 못하면 바로 끝낸다. 적은 착지 후 일반 상태로 돌아간다.
	if (!AmbushMontage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	HitActors.Reset();

	// 착지 시점이나 노티파이 시점의 Hit 이벤트를 기다린다. 몽타주 앞부분 노티파이를 놓치지 않도록 재생보다 먼저 건다.
	UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		UGABaseAttack::GetHitEventTag()
	);
	HitTask->EventReceived.AddDynamic(this, &UGACeilingAmbush::OnHitEvent);
	HitTask->ReadyForActivation();

	// 블렌드 인 시간은 몽타주에 들어 있어, 매달린 자세에서 기습 자세로 자연스럽게 넘어간다.
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		AmbushMontage,
		Enemy->GetAmbushPlayRate(),
		NAME_None,
		true,
		1.0f,
		Enemy->GetAmbushStartTime()
	);
	MontageTask->OnCompleted.AddDynamic(this, &UGACeilingAmbush::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UGACeilingAmbush::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGACeilingAmbush::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UGACeilingAmbush::OnMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UGACeilingAmbush::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	HitActors.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGACeilingAmbush::OnMontageCompleted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
	}
}

void UGACeilingAmbush::OnMontageInterrupted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
	}
}

void UGACeilingAmbush::OnHitEvent(FGameplayEventData Payload)
{
	ACeilingEnemy* Enemy = Cast<ACeilingEnemy>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();

	if (!IsActive() || !Enemy || Enemy->IsDead() || !SourceASC || !SourceASC->GetSet<UStatAttributeSet>())
	{
		return;
	}

	// 공격력에 배율을 곱한 값을 피해로 사용한다.
	const float Damage = SourceASC->GetNumericAttribute(UStatAttributeSet::GetAttackPowerAttribute()) * DamageMultiplier;
	if (!FMath::IsFinite(Damage) || Damage <= 0.0f)
	{
		return;
	}

	// 캡슐 바닥(발밑)을 중심으로 폰을 찾는다.
	const float Radius = FMath::Max(1.0f, HitRadius);
	const FVector Center = Enemy->GetActorLocation()
		- FVector(0.0f, 0.0f, Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CeilingAmbush), false, Enemy);

	TArray<FOverlapResult> Overlaps;
	Enemy->GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		Center,
		FQuat::Identity,
		ObjectParams,
		FCollisionShape::MakeSphere(Radius),
		QueryParams
	);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();

		// 같은 대상은 한 번만 맞고, 다른 적은 맞지 않는다.
		if (!IsValid(Target) || HitActors.Contains(Target) || !Enemy->IsValidAITarget(Target))
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC || !TargetASC->GetSet<UStatAttributeSet>())
		{
			continue;
		}

		if (TargetASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f)
		{
			continue;
		}

		// 기본 공격과 같은 GE에 SetByCaller로 피해량을 넘긴다.
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
			UGEBasicAttackDamage::StaticClass(),
			GetAbilityLevel(),
			Context
		);

		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Damage")), Damage);
			HitActors.Add(Target);
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		}
	}
}
