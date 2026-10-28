



#include "GAS/Ability/GASwiftStrike.h"
#include "NativeGameplayTags.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Ability/GAAssassinateBase.h"
#include "GAS/Ability/GAWallAttach.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/Enemy.h"
#include "GAS/StatAttributeSet.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SwiftStrike, "Ability.Attack.SwiftStrike");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SwiftStriking, "State.SwiftStriking");
// 기본공격·쿠나이 등 공격 어빌리티의 공통 부모. 땅에서 질풍참이 이들을 끊고 들어간다.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AttackAbility, "Ability.Attack");

UGASwiftStrike::UGASwiftStrike()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_SwiftStrike);
	SetAssetTags(Tags);

	// 돌진하는 동안 기본공격·쿠나이·암살·불속성이 끼어들지 못하게 공격 중 태그를 가진다
	ActivationOwnedTags.AddTag(TAG_SwiftStriking);
	ActivationOwnedTags.AddTag(UGABaseAttack::GetAttackingTag());
	ActivationBlockedTags.AddTag(TAG_SwiftStriking);

	// 땅에서는 우선순위가 가장 높다. 공격 중 태그로 막지 않고, 진행 중인 공격을 끊는다 (ActivateAbility 전에 취소된다).
	// 공중에서 공격 중일 때는 CanActivateAbility에서 막는다. 암살은 끊지 않는다.
	CancelAbilitiesWithTag.AddTag(TAG_AttackAbility);
	ActivationBlockedTags.AddTag(UGAAssassinateBase::GetAssassinatingTag());

	// 벽에 붙음·모서리 매달림·오르기 중에는 쓸 수 없다
	ActivationBlockedTags.AddTag(UGAWallAttach::GetWallStateTag());
}

bool UGASwiftStrike::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	if (!ActorInfo) return false;

	const ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(Character) || !IsValid(ASC)) return false;

	//// 공중에서는 다른 공격을 끊지 않는다
	//const bool bOnGround = Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsMovingOnGround();
	//if (!bOnGround && ASC->HasMatchingGameplayTag(UGABaseAttack::GetAttackingTag())) return false;

	return true;
}

void UGASwiftStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);


	FHitResult Hit;

	ACharacter* Player = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Player)) return;

	UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float halfHeight = Capsule->GetScaledCapsuleHalfHeight();

	FVector Start = Player->GetActorLocation();
	FVector Dir = Player->GetActorForwardVector();
	FVector End = Start + Dir * DashDistance;
	FCollisionShape Shape = FCollisionShape::MakeCapsule(FMath::Max(1.f, Radius - 2.f), FMath::Max(1.f, halfHeight - 2.f));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SwiftStrike), false, Player);
	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);

	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, Shape, Params, ResponseParams))
	{
		if (!Hit.bStartPenetrating)
		{
			// TODO
		}

		End = Hit.Location;
	}

	const float ActualDistance = FVector::Dist2D(Start, End);
	const float MoveDuration = FMath::Max(0.03f, Duration * ActualDistance / DashDistance);

	UAbilityTask_ApplyRootMotionMoveToForce* MoveTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this,
		NAME_None,
		End,
		MoveDuration,
		false,
		MOVE_Walking,
		false,
		nullptr,
		ERootMotionFinishVelocityMode::SetVelocity,
		FVector::ZeroVector,
		0.0f
	);

	AttackTargets.Reset();
	CollectTargets(Player, Start, End, Shape);

	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	MoveTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UGASwiftStrike::OnSwiftStrikeFinished);
	MoveTask->OnTimedOut.AddDynamic(this, &UGASwiftStrike::OnSwiftStrikeFinished);
	MoveTask->ReadyForActivation();


	LoadedSwiftStrikeMontage = SwiftStrikeMontage.LoadSynchronous();
	if (IsValid(LoadedSwiftStrikeMontage))
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, LoadedSwiftStrikeMontage, 2.0f, NAME_None, true);
		MontageTask->ReadyForActivation();
	}
}

void UGASwiftStrike::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{

	ACharacter* Player = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Player)) return;

	UCapsuleComponent* Capsule = Player->GetCapsuleComponent();

	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGASwiftStrike::OnSwiftStrikeFinished()
{
	if (!IsActive()) return;
	ApplyHitToTargets();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGASwiftStrike::OnSwiftStrikeInterrupted()
{
	if (!IsActive()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

void UGASwiftStrike::CollectTargets(const ACharacter* Player, const FVector& Start, const FVector& End, const FCollisionShape& Shape)
{
	if (!Player) return;

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SwiftStrike), false, Player);
	FCollisionObjectQueryParams ObjectParam(ECC_Pawn);

	GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, ObjectParam, Shape, Params);

	TSet<const AActor*> Seen;
	for (const FHitResult& Hit : Hits)
	{
		const AEnemy* Enemy = Cast<AEnemy>(Hit.GetActor());
		if (!IsValid(Enemy) || Enemy->IsDead() || Seen.Contains(Enemy)) continue;

		AttackTargets.Add(Hit);
		Seen.Add(Enemy);
	}
}

void UGASwiftStrike::ApplyHitToTargets()
{
	for (const FHitResult Hit : AttackTargets)
	{
		UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Hit.GetActor());
		if (!SourceASC || !TargetASC || !TargetASC->GetSet<UStatAttributeSet>()
			|| TargetASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f)
		{
			return;
		}

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddHitResult(Hit);

		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffect, GetAbilityLevel(), Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Damage")), Damage);
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		}
	}

}

FGameplayTag UGASwiftStrike::GetSwiftStrikeAbilityTag()
{
	return TAG_SwiftStrike;
}

FGameplayTag UGASwiftStrike::GetSwiftStrikingTag()
{
	return TAG_SwiftStriking;
}
