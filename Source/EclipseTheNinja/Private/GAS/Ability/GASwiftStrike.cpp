



#include "GAS/Ability/GASwiftStrike.h"
#include "NativeGameplayTags.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Ability/GAAssassinateBase.h"
#include "GAS/Ability/GAWallAttach.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "CollisionShape.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/Enemy.h"
#include "GAS/StatAttributeSet.h"
#include "GAS/Tags/EclipseGameplayTags.h"

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
	// 무적은 돌진 구간에서만 별도로 부여하고 해제한다.
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

	bDashFinished = false;
	bMontageFinished = true;
	bUsingAirMontage = false;

	FHitResult Hit;

	ACharacter* Player = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Player))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
	if (!IsValid(Capsule))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

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
	DrawDebugHitbox(Start, End, Shape);

	SavedPawnCollisionResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	bDashCollisionChanged = true;

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (IsValid(SourceASC))
	{
		SourceASC->AddLooseGameplayTag(EclipseGameplayTags::State_Invincible);
		bDashInvincibilityAdded = true;
	}

	MoveTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UGASwiftStrike::OnSwiftStrikeFinished);
	MoveTask->OnTimedOut.AddDynamic(this, &UGASwiftStrike::OnSwiftStrikeFinished);

	// 짧은 돌진이 먼저 끝나더라도 몽타주 완료를 기다릴 수 있도록 먼저 재생한다.
	TSoftObjectPtr<UAnimMontage> SelectedMontage = SwiftStrikeMontage;
	const UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	if (IsValid(Movement) && Movement->IsFalling())
	{
		if (!AirSwiftStrikeMontage.IsNull())
		{
			SelectedMontage = AirSwiftStrikeMontage;
			bUsingAirMontage = true;
		}
	}

	LoadedSwiftStrikeMontage = SelectedMontage.LoadSynchronous();
	if (IsValid(LoadedSwiftStrikeMontage))
	{
		const FName DashSection(TEXT("Dash"));
		const FName RecoverySection(TEXT("Recovery"));
		const int32 DashSectionIndex = LoadedSwiftStrikeMontage->GetSectionIndex(DashSection);
		FName StartSection = NAME_None;
		float MontagePlayRate = 2.0f;
		if (DashSectionIndex != INDEX_NONE
			&& LoadedSwiftStrikeMontage->GetSectionIndex(RecoverySection) != INDEX_NONE)
		{
			StartSection = DashSection;

			// 돌진 구간을 되감지 않고 실제 이동 시간에 맞춰 한 번만 재생한다.
			const float DashSectionLength = LoadedSwiftStrikeMontage->GetSectionLength(DashSectionIndex);
			const float MontageRateScale = LoadedSwiftStrikeMontage->RateScale;
			if (DashSectionLength > KINDA_SMALL_NUMBER && MontageRateScale > KINDA_SMALL_NUMBER)
			{
				MontagePlayRate = DashSectionLength / (MoveDuration * MontageRateScale);
			}
		}

		bMontageFinished = false;
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, LoadedSwiftStrikeMontage, MontagePlayRate, StartSection, true);
		MontageTask->OnCompleted.AddDynamic(this, &UGASwiftStrike::OnSwiftStrikeMontageCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &UGASwiftStrike::OnSwiftStrikeInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UGASwiftStrike::OnSwiftStrikeInterrupted);
		MontageTask->ReadyForActivation();

		// 에셋에 반복 연결이 있어도 돌진 포즈에서 마무리 포즈로 한 번만 이어진다.
		if (IsActive() && StartSection == DashSection)
		{
			UAnimInstance* Animation = ActorInfo->GetAnimInstance();
			if (IsValid(Animation))
			{
				Animation->Montage_SetNextSection(DashSection, RecoverySection, LoadedSwiftStrikeMontage);
				Animation->Montage_SetNextSection(RecoverySection, NAME_None, LoadedSwiftStrikeMontage);
			}
		}
	}

	// 몽타주 재생 실패로 취소되었다면 이동을 시작하지 않는다.
	if (!IsActive()) return;
	MoveTask->ReadyForActivation();
}

void UGASwiftStrike::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RestoreDashState();
	AttackTargets.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGASwiftStrike::OnSwiftStrikeFinished()
{
	if (!IsActive()) return;
	if (bDashFinished) return;

	bDashFinished = true;
	RestoreDashState();

	ApplyHitToTargets();
	AttackTargets.Reset();

	if (!IsActive()) return;
	if (bMontageFinished)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
		return;
	}

	// Recovery가 없는 기존 몽타주는 끝까지 재생하고, 있으면 마무리 구간으로 넘긴다.
	if (!IsValid(LoadedSwiftStrikeMontage)) return;
	const FName RecoverySection(TEXT("Recovery"));
	if (LoadedSwiftStrikeMontage->GetSectionIndex(RecoverySection) == INDEX_NONE) return;

	UAnimInstance* Animation = GetCurrentActorInfo()->GetAnimInstance();
	if (!IsValid(Animation)) return;

	Animation->Montage_SetNextSection(RecoverySection, NAME_None, LoadedSwiftStrikeMontage);
	float RecoveryPlayRate = 2.0f;
	if (bUsingAirMontage)
	{
		// 공중에서는 짧게 포즈를 마무리하고 낙하 애니메이션으로 돌아간다.
		RecoveryPlayRate = 6.0f;
	}
	Animation->Montage_SetPlayRate(LoadedSwiftStrikeMontage, RecoveryPlayRate);
	// 이미 마무리 동작에 들어갔다면 시작 프레임으로 다시 되감지 않는다.
	if (Animation->Montage_GetCurrentSection(LoadedSwiftStrikeMontage) != RecoverySection)
	{
		Animation->Montage_JumpToSection(RecoverySection, LoadedSwiftStrikeMontage);
	}
}

void UGASwiftStrike::OnSwiftStrikeMontageCompleted()
{
	if (!IsActive()) return;

	bMontageFinished = true;
	// 몽타주가 먼저 끝나더라도 아직 진행 중인 돌진을 끊지 않는다.
	if (!bDashFinished) return;

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGASwiftStrike::RestoreDashState()
{
	if (bDashInvincibilityAdded)
	{
		bDashInvincibilityAdded = false;
		UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
		if (IsValid(SourceASC))
		{
			// 이 어빌리티가 더한 개수만 제거해 다른 무적 효과는 유지한다.
			SourceASC->RemoveLooseGameplayTag(EclipseGameplayTags::State_Invincible);
		}
	}

	if (!bDashCollisionChanged) return;
	bDashCollisionChanged = false;

	ACharacter* Player = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Player)) return;

	UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
	if (!IsValid(Capsule)) return;

	Capsule->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnCollisionResponse);
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

void UGASwiftStrike::DrawDebugHitbox(const FVector& Start, const FVector& End, const FCollisionShape& Shape) const
{
	if (!bDrawDebugHitbox) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const float Radius = Shape.GetCapsuleRadius();
	const float HalfHeight = Shape.GetCapsuleHalfHeight();
	// 맞은 적이 있으면 빨강, 없으면 초록
	const FColor Color = AttackTargets.Num() > 0 ? FColor::Red : FColor::Green;

	DrawDebugCapsule(World, Start, HalfHeight, Radius, FQuat::Identity, FColor::White, false, DebugDrawTime, 0, 1.f);
	DrawDebugCapsule(World, End, HalfHeight, Radius, FQuat::Identity, Color, false, DebugDrawTime, 0, 1.5f);

	// 캡슐이 쓸고 지나간 영역의 좌우·위아래 외곽선
	const FVector Dir = (End - Start).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir) * Radius;
	const FVector Up = FVector::UpVector * HalfHeight;
	DrawDebugLine(World, Start + Side, End + Side, Color, false, DebugDrawTime, 0, 1.5f);
	DrawDebugLine(World, Start - Side, End - Side, Color, false, DebugDrawTime, 0, 1.5f);
	DrawDebugLine(World, Start + Up, End + Up, Color, false, DebugDrawTime, 0, 1.5f);
	DrawDebugLine(World, Start - Up, End - Up, Color, false, DebugDrawTime, 0, 1.5f);

	for (const FHitResult& Hit : AttackTargets)
	{
		DrawDebugPoint(World, Hit.ImpactPoint, 14.f, FColor::Red, false, DebugDrawTime);
		if (const AActor* Target = Hit.GetActor())
		{
			DrawDebugString(World, Target->GetActorLocation() + FVector(0.f, 0.f, HalfHeight + 20.f),
				TEXT("SwiftStrike Hit"), nullptr, FColor::Red, DebugDrawTime, true);
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
