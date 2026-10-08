


#include "GAS/Ability/GABaseAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GAS/Effects/GEBasicAttackDamage.h"
#include "GAS/StatAttributeSet.h"
#include "NativeGameplayTags.h"
#include "Character/BaseCharacter.h"
#include "Character/Enemy.h"
#include "GameFramework/Character.h"

namespace
{
	UAnimMontage* GetBasicAttackMontage(const ACharacter* Character)
	{
		if (const ABaseCharacter* CombatCharacter = Cast<ABaseCharacter>(Character))
		{
			return CombatCharacter->GetBasicAttackMontage();
		}
		return nullptr;
	}
}

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_BasicAttack, "Ability.Attack.Basic");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Attacking, "State.Attacking");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_BasicAttackHit, "Event.Attack.Basic.Hit");

UGABaseAttack::UGABaseAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_BasicAttack);
	SetAssetTags(Tags);
	ActivationOwnedTags.AddTag(TAG_Attacking);
	ActivationBlockedTags.AddTag(TAG_Attacking);
}

FGameplayTag UGABaseAttack::GetHitEventTag()
{
	return TAG_BasicAttackHit;
}

bool UGABaseAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const AEnemy* Enemy = Cast<AEnemy>(Character);
	return Character && GetBasicAttackMontage(Character) && Character->GetMesh()->GetAnimInstance()
		&& SourceASC && SourceASC->GetSet<UStatAttributeSet>()
		&& SourceASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) > 0.0f
		&& (!Enemy || (!Enemy->IsDead() && Enemy->GetBehaviorState() == EEnemyBehaviorState::Attack
			&& Enemy->GetCombatTarget()));
}

void UGABaseAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	UAnimMontage* AttackMontage = GetBasicAttackMontage(Character);
	if (!Character || !AttackMontage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	HitActors.Reset();

	// Listen before playing: a notify may occur at the start of the montage.
	UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TAG_BasicAttackHit);
	HitTask->EventReceived.AddDynamic(this, &UGABaseAttack::OnHitEvent);
	HitTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, AttackMontage, FMath::Max(0.01f, PlayRate), NAME_None, true,
		1.0f, 0.0f, true);
	MontageTask->OnCompleted.AddDynamic(this, &UGABaseAttack::OnAttackCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGABaseAttack::OnAttackInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UGABaseAttack::OnAttackInterrupted);
	MontageTask->ReadyForActivation();
}

void UGABaseAttack::OnAttackCompleted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
	}
}

void UGABaseAttack::OnAttackInterrupted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
	}
}

void UGABaseAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	HitActors.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGABaseAttack::OnHitEvent(FGameplayEventData Payload)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsActive() || !Character || !SourceASC
		|| SourceASC->GetAnimatingAbility() != this || !SourceASC->GetSet<UStatAttributeSet>())
	{
		return;
	}
	const AEnemy* Enemy = Cast<AEnemy>(Character);
	if (SourceASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f || (Enemy && Enemy->IsDead()))
	{
		OnAttackInterrupted();
		return;
	}
	const float Damage = SourceASC->GetNumericAttribute(UStatAttributeSet::GetAttackPowerAttribute());
	if (!FMath::IsFinite(Damage) || Damage <= 0.0f)
	{
		return;
	}
	const float Radius = FMath::Max(1.0f, AttackRadius);
	const FVector Start = Character->GetActorLocation() + Character->GetActorForwardVector() * Radius;
	const FVector End = Character->GetActorLocation() + Character->GetActorForwardVector() * FMath::Max(Radius, AttackRange);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BasicAttack), false, Character);
	TArray<FHitResult> Hits;
	Character->GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity,
		ObjectParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!IsValid(Target) || Target == Character || HitActors.Contains(Target))
		{
			continue;
		}
		// Enemy attacks only its currently tracked player, never another enemy.
		if (Enemy && Target != Enemy->GetCombatTarget())
		{
			continue;
		}
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC || !TargetASC->GetSet<UStatAttributeSet>()
			|| TargetASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f)
		{
			continue;
		}
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddHitResult(Hit);
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UGEBasicAttackDamage::StaticClass(), GetAbilityLevel(), Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Damage")), Damage);
			HitActors.Add(Target);
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		}
	}
}

