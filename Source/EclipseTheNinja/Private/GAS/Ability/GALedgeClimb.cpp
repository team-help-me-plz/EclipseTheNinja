


#include "GAS/Ability/GALedgeClimb.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/PlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveVector.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Ability/GALedgeHang.h"
#include "GAS/Ability/GAWallAttach.h"
#include "GAS/StatAttributeSet.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LedgeClimb, "Ability.Movement.LedgeClimb");

UGALedgeClimb::UGALedgeClimb()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_LedgeClimb);
	SetAssetTags(Tags);

	// 매달린 상태에서만 올라간다. 시작하면 매달리기는 취소된다 (ActivateAbility 전에 끝난다).
	ActivationRequiredTags.AddTag(UGAWallAttach::GetWallAttachingTag());
	CancelAbilitiesWithTag.AddTag(UGAWallAttach::GetWallAttachAbilityTag());

	// 올라가는 동안 공격·암살과 이동·커서 회전을 막는다
	ActivationOwnedTags.AddTag(UGALedgeHang::GetLedgeHangingTag());
	ActivationOwnedTags.AddTag(UGABaseAttack::GetAttackingTag());
	ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());
}

bool UGALedgeClimb::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	if (!ActorInfo) return false;

	const APlayerCharacter* Player = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Player)) return false;

	// 플레이어가 죽은 경우 실행 불가
	const UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(SourceASC)) return false;
	if (SourceASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f) return false;

	FVector TestStand;
	float TestFacing = 0.f;
	return FindClimb(Player, TestStand, TestFacing);
}

void UGALedgeClimb::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	bMovementChanged = false;
	bReachedStand = false;

	// 매달리기가 취소되며 Falling으로 바뀌고 메시 오프셋·정지 자세가 풀린 상태다
	APlayerCharacter* Player = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	float Facing = 1.f;
	if (!IsValid(Player) || !FindClimb(Player, StandLocation, Facing))
	{
		OnInterrupted();
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		OnInterrupted();
		return;
	}

	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Flying);
	bMovementChanged = true;

	// 올라가는 쪽(벽 쪽)을 바라본다
	Player->SetAssassinationFacing(Facing > 0.f ? 0.f : 180.f);

	LoadedClimbMontage = ClimbMontage.LoadSynchronous();
	if (IsValid(LoadedClimbMontage))
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, LoadedClimbMontage, 1.0f, NAME_None, true);
		MontageTask->ReadyForActivation();

		// 위치는 코드가 맡으므로 몽타주 루트 모션은 끈다
		UAnimInstance* AnimInstance = Player->GetMesh() ? Player->GetMesh()->GetAnimInstance() : nullptr;
		if (FAnimMontageInstance* MontageInstance = AnimInstance ? AnimInstance->GetActiveInstanceForMontage(LoadedClimbMontage) : nullptr)
		{
			MontageInstance->PushDisableRootMotion();
		}
	}

	if (!IsValid(PathCurve))
	{
		PathCurve = NewObject<UCurveVector>(this);
	}

	// 루트 모션 궤적이 있으면 따라간다 (먼저 올라간 뒤 안쪽으로 넘어간다).
	// 없으면 직선 대각선 이동은 모서리에 걸리므로, 전반에 위로 올라가고 후반에 안쪽으로 넘어가는 곡선을 쓴다.
	float Duration = ClimbTime;
	if (!UGALedgeHang::BuildRootMotionPath(LoadedClimbMontage, Player->GetActorLocation(), StandLocation, PathCurve, Duration))
	{
		Duration = ClimbTime;
		FRichCurve& ForwardCurve = PathCurve->FloatCurves[0];
		FRichCurve& UpCurve = PathCurve->FloatCurves[2];
		ForwardCurve.Reset();
		UpCurve.Reset();
		const FVector Delta = StandLocation - Player->GetActorLocation();
		const float Horizontal = Delta.Size2D();
		const float Vertical = Delta.Z;
		ForwardCurve.AddKey(0.f, 0.f);
		ForwardCurve.AddKey(0.6f, -0.6f * Horizontal);
		ForwardCurve.AddKey(1.f, 0.f);
		UpCurve.AddKey(0.f, 0.f);
		UpCurve.AddKey(0.6f, 0.4f * Vertical);
		UpCurve.AddKey(1.f, 0.f);
	}

	UAbilityTask_ApplyRootMotionMoveToForce* MoveTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this, NAME_None, StandLocation, Duration, false, MOVE_Flying, false, PathCurve,
		ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.0f);
	MoveTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UGALedgeClimb::OnClimbFinished);
	MoveTask->OnTimedOut.AddDynamic(this, &UGALedgeClimb::OnClimbFinished);
	MoveTask->ReadyForActivation();
}

void UGALedgeClimb::OnClimbFinished()
{
	if (!IsActive()) return;

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	// 모서리에 걸려 올라서지 못했으면 떨어진다
	bReachedStand = IsValid(Avatar) && FVector::DistSquared(Avatar->GetActorLocation(), StandLocation) <= FMath::Square(30.0f);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, !bReachedStand);
}

void UGALedgeClimb::OnInterrupted()
{
	if (!IsActive()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

void UGALedgeClimb::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ACharacter* Player = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const bool bStand = bReachedStand && !bWasCancelled;
	const bool bRestoreMovement = bMovementChanged;
	bReachedStand = false;
	bMovementChanged = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (!IsValid(Player) || !bRestoreMovement) return;

	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->StopMovementImmediately();

	if (bStand)
	{
		// 설 자리에 정확히 놓는다
		Player->SetActorLocation(StandLocation, true);

		// 매달리기 메시 오프셋이 남아 있으면 메시가 떠 보이므로 캐릭터 기본 위치로 확정한다
		const ACharacter* DefaultCharacter = Player->GetClass()->GetDefaultObject<ACharacter>();
		if (Player->GetMesh() && DefaultCharacter && DefaultCharacter->GetMesh())
		{
			Player->GetMesh()->SetRelativeLocation(DefaultCharacter->GetMesh()->GetRelativeLocation());
		}
	}

	// 바로 걷기로 바꾸면 바닥과 조금만 어긋나도 떠 있게 되므로, 낙하로 넘겨 바닥에 착지하게 한다
	Movement->SetMovementMode(MOVE_Falling);

	UE_LOG(LogTemp, Log, TEXT("LedgeClimb end: stand=%d actor=%s target=%s mesh=%s"),
		bStand ? 1 : 0, *Player->GetActorLocation().ToCompactString(), *StandLocation.ToCompactString(),
		Player->GetMesh() ? *Player->GetMesh()->GetRelativeLocation().ToCompactString() : TEXT("-"));
}

bool UGALedgeClimb::FindClimb(const ACharacter* Player, FVector& OutStandLocation, float& OutFacing) const
{
	const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Location = Player->GetActorLocation();

	// 매달린 동안에는 벽을 바라보고 있다
	OutFacing = Player->GetActorForwardVector().X >= 0.f ? 1.f : -1.f;
	const FVector Dir(OutFacing, 0.f, 0.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LedgeClimb), false, Player);
	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse = Capsule->GetCollisionResponseToChannels();
	const ECollisionChannel PawnChannel = Capsule->GetCollisionObjectType();

	// 1. 벽면 위치
	FHitResult WallHit;
	if (!GetWorld()->LineTraceSingleByChannel(WallHit, Location, Location + Dir * (Radius + 30.f), PawnChannel, Params, ResponseParams)) return false;
	const float WallX = WallHit.ImpactPoint.X;

	// 2. 모서리 윗면: 설 자리 위에서 아래로 트레이스. 손이 닿는 높이 안에 걸어 다닐 수 있는 면이 있어야 한다.
	const float StandX = WallX + OutFacing * (Radius + ClimbInset);
	FHitResult TopHit;
	const FVector TopStart(StandX, Location.Y, Location.Z + MaxReachHeight);
	const FVector TopEnd(StandX, Location.Y, Location.Z);
	if (!GetWorld()->LineTraceSingleByChannel(TopHit, TopStart, TopEnd, PawnChannel, Params, ResponseParams)) return false;
	if (TopHit.bStartPenetrating) return false;
	if (TopHit.ImpactNormal.Z < 0.7f) return false;

	OutStandLocation = FVector(StandX, Location.Y, TopHit.ImpactPoint.Z + HalfHeight + 1.f);

	// 3. 올라가는 경로(위로 → 안쪽으로)와 설 자리가 막혀 있지 않아야 한다
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(FMath::Max(1.f, Radius - 2.f), FMath::Max(1.f, HalfHeight - 2.f));
	const FVector AboveLedge(Location.X, Location.Y, OutStandLocation.Z + 2.f);
	FHitResult PathHit;
	if (GetWorld()->SweepSingleByChannel(PathHit, Location, AboveLedge, FQuat::Identity, PawnChannel, Shape, Params, ResponseParams)) return false;
	if (GetWorld()->SweepSingleByChannel(PathHit, AboveLedge, OutStandLocation + FVector(0.f, 0.f, 2.f), FQuat::Identity, PawnChannel, Shape, Params, ResponseParams)) return false;

	return true;
}
