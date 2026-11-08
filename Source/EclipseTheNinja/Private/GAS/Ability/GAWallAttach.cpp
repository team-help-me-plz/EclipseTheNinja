


#include "GAS/Ability/GAWallAttach.h"
#include "NativeGameplayTags.h"
#include "GAS/Ability/GABaseAttack.h"
#include "Character/PlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "GAS/StatAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "GAS/Ability/GALedgeHang.h"
#include "GAS/Ability/GALedgeClimb.h"
#include "Util/EclipseCollisionChannels.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallAttach, "Ability.Movement.WallAttach");
// 벽 매달리기·모서리 매달리기·오르기 상태의 공통 부모. 막을 때는 이 태그 하나로 막는다.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallState, "State.Movement.Wall");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallAttached, "State.Movement.Wall.Attached");
static const ECollisionChannel ECC_WallAttach = ECC_GameTraceChannel2;
static const ECollisionChannel ECC_WallClimbing = ECC_GameTraceChannel4;
static constexpr float ClimbDebugInterval = 0.05f;

UGAWallAttach::UGAWallAttach()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	// 암살이 시작될 때 이 태그로 매달리기를 취소한다
	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_WallAttach);
	SetAssetTags(Tags);

	// 매달린 상태에서 공중·아래 암살은 가능해야 하므로 공격중 태그는 붙이지 않는다.
	// 기본 공격은 GABaseAttack이 WallAttached 태그로 막는다.
	ActivationOwnedTags.AddTag(TAG_WallAttached);
	// 공격 중에는 벽에 붙지 않도록
	ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());
}

FGameplayTag UGAWallAttach::GetWallAttachAbilityTag()
{
	return TAG_WallAttach;
}

FGameplayTag UGAWallAttach::GetWallAttachingTag()
{
	return TAG_WallAttached;
}

FGameplayTag UGAWallAttach::GetWallStateTag()
{
	return TAG_WallState;
}


bool UGAWallAttach::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	if (!ActorInfo) return false;

	const APlayerCharacter* Player = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Player)) return false;

	// 플레이어가 죽은 경우 실행 불가
	const UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(SourceASC)) return false;
	if (SourceASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f) return false;

	// 땅에서 벽에 매달리기를 허용하지 않는 경우 공중에 있는지까지 확인
	if (!bCanAttachOnGround && Player->GetCharacterMovement()->IsMovingOnGround()) return false;

	FHitResult Hit;
	return FindWall(Player, Hit);
}

void UGAWallAttach::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APlayerCharacter* Player = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	FHitResult WallHit;
	bool bClimbable = false;
	if (!IsValid(Player) || !FindWall(Player, WallHit, &bClimbable))
	{
		OnInterrupted();
		return;
	}
	SetClimbable(bClimbable);

	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Flying);
	//Movement->DisableMovement();

	// 캡슐은 벽 표면에 딱 붙인다 (벽 속에 넣으면 Falling 복귀 시 튕겨 나옴)
	const float WallNormalX = WallHit.ImpactNormal.X > 0.f ? 1.f : -1.f;
	const float Radius = Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
	FVector AttachPoint = Player->GetActorLocation();
	AttachPoint.X = WallHit.ImpactPoint.X + WallNormalX * Radius;
	Player->SetActorLocation(AttachPoint);

	// 벽을 정면으로 바라보게 해서 메시 오프셋의 전방이 항상 벽 쪽이 되도록
	Player->SetActorRotation(FRotator(0.f, WallNormalX > 0.f ? 180.f : 0.f, 0.f));

#if ENABLE_DRAW_DEBUG
	// 매달린 동안 올라가기 가능 범위를 계속 보여 준다 (GALedgeClimb의 bDrawDebugClimb)
	GetWorld()->GetTimerManager().SetTimer(ClimbDebugTimer, this, &UGAWallAttach::TickClimbDebug, ClimbDebugInterval, true, 0.f);
#endif

	// 모서리에서 내려와 매달린 경우는 내려가기 끝 자세를 정지 몽타주로 붙잡고 있으므로 메시 오프셋을 주지 않는다.
	// (자세와 위치가 이미 맞아 있어 오프셋을 주면 메시가 튄다)
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	bHoldingLedgePose = IsValid(ASC) && ASC->HasMatchingGameplayTag(UGALedgeHang::GetLedgeHoldTag());
	if (bHoldingLedgePose)
	{
		ASC->RemoveLooseGameplayTag(UGALedgeHang::GetLedgeHoldTag());

		// 내려가기의 손 맞추기로 메시가 옮겨져 있을 수 있으므로, 끝날 때 기본 위치로 되돌리게 한다
		const ACharacter* DefaultCharacter = Player->GetClass()->GetDefaultObject<ACharacter>();
		if (Player->GetMesh() && DefaultCharacter && DefaultCharacter->GetMesh())
		{
			BaseMeshRelativeLocation = DefaultCharacter->GetMesh()->GetRelativeLocation();
			bMeshOffsetApplied = !Player->GetMesh()->GetRelativeLocation().Equals(BaseMeshRelativeLocation, 0.1f);
		}
		return;
	}

	// 애니메이션 위치 보정은 캡슐이 아니라 메시에서 한다.
	// 기준은 캐릭터 클래스 기본값으로 잡아, 이전 매달리기의 되돌리기가 진행 중이어도 오프셋이 쌓이지 않게 한다.
	const ACharacter* DefaultCharacter = Player->GetClass()->GetDefaultObject<ACharacter>();
	if (Player->GetMesh() && DefaultCharacter && DefaultCharacter->GetMesh())
	{
		BaseMeshRelativeLocation = DefaultCharacter->GetMesh()->GetRelativeLocation();

		// 모서리에서 내려오며 손 맞추기로 메시를 이미 매달리기 위치에 뒀으면 그대로 두고, 끝날 때만 되돌린다
		if (IsValid(ASC) && ASC->HasMatchingGameplayTag(UGALedgeHang::GetLedgeMeshAlignedTag()))
		{
			ASC->RemoveLooseGameplayTag(UGALedgeHang::GetLedgeMeshAlignedTag());
			bMeshOffsetApplied = true;
			return;
		}

		// 모서리에서 내려와 매달린 경우는 GALedgeHang이 높이 오프셋을 이미 줬으므로 벽 쪽(X)만 섞인다
		BlendMeshTo(Player, BaseMeshRelativeLocation + FVector(AnimXOffset, 0.f, AnimZOffset));
		bMeshOffsetApplied = true;
	}
}

void UGAWallAttach::BlendMeshTo(ACharacter* Player, const FVector& Target)
{
	UWorld* World = Player->GetWorld();
	World->GetTimerManager().ClearTimer(MeshBlendTimer);

	if (MeshOffsetBlendTime <= KINDA_SMALL_NUMBER)
	{
		Player->GetMesh()->SetRelativeLocation(Target);
		return;
	}

	BlendCharacter = Player;
	MeshBlendFrom = Player->GetMesh()->GetRelativeLocation();
	MeshBlendTo = Target;
	MeshBlendStartTime = World->GetTimeSeconds();
	World->GetTimerManager().SetTimer(MeshBlendTimer, this, &UGAWallAttach::TickMeshBlend, 0.01f, true);
}

void UGAWallAttach::TickMeshBlend()
{
	ACharacter* Player = BlendCharacter.Get();
	if (!IsValid(Player) || !Player->GetMesh())
	{
		if (IsValid(Player)) Player->GetWorld()->GetTimerManager().ClearTimer(MeshBlendTimer);
		return;
	}

	const float Alpha = FMath::Clamp(static_cast<float>(Player->GetWorld()->GetTimeSeconds() - MeshBlendStartTime) / MeshOffsetBlendTime, 0.f, 1.f);
	Player->GetMesh()->SetRelativeLocation(FMath::Lerp(MeshBlendFrom, MeshBlendTo, FMath::SmoothStep(0.f, 1.f, Alpha)));
	if (Alpha >= 1.f)
	{
		Player->GetWorld()->GetTimerManager().ClearTimer(MeshBlendTimer);
	}
}

void UGAWallAttach::TickClimbDebug()
{
	const ACharacter* Player = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsActive() || !IsValid(Player) || !IsValid(ASC)) return;

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (const UGALedgeClimb* Climb = Cast<UGALedgeClimb>(Spec.GetPrimaryInstance()))
		{
			// 다음 타이머까지 사라지지 않게 간격보다 조금 길게 남긴다
			Climb->DrawClimbDebug(Player, ClimbDebugInterval * 1.5f);
			return;
		}
	}
}

void UGAWallAttach::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	const bool bReturnMesh = bMeshOffsetApplied;
	if (IsValid(Player))
	{
		UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
		Movement->SetMovementMode(MOVE_Falling);

		// 모서리 매달리기의 정지 자세를 풀어 준다. 이어서 시작하는 올라가기·암살·벽점프 몽타주가 섞여 들어온다.
		if (bHoldingLedgePose && Player->GetMesh())
		{
			if (UAnimInstance* AnimInstance = Player->GetMesh()->GetAnimInstance())
			{
				AnimInstance->Montage_Stop(0.2f);
			}
		}
	}
	bMeshOffsetApplied = false;
	bHoldingLedgePose = false;
	SetClimbable(false);

	// 엔진의 EndAbility는 이 어빌리티의 타이머를 모두 지우므로, 메시 되돌리기는 그 뒤에 시작한다
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// 이어서 시작하는 올라가기·암살·벽점프 몽타주가 섞여 들어오는 동안 메시를 원래 위치로 되돌린다
	if (bReturnMesh && IsValid(Player) && Player->GetMesh())
	{
		BlendMeshTo(Player, BaseMeshRelativeLocation);
	}
}

void UGAWallAttach::OnCompleted()
{
	if (!IsActive()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGAWallAttach::OnInterrupted()
{
	if (!IsActive()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

bool UGAWallAttach::CanClimbToward(const ACharacter* Player, float Up) const
{
	if (!IsValid(Player) || FMath::IsNearlyZero(Up)) return true;

	// 매달린 동안에는 벽을 바라보고 있다
	const float SearchDir = Player->GetActorForwardVector().X > 0 ? 1.f : -1.f;
	const FVector ForwardVector(SearchDir, 0.f, 0.f);

	// 움직이는 쪽 캡슐 끝에서 조금 더 나아간 높이의 벽을 확인한다
	const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector ProbeStart = Player->GetActorLocation();
	ProbeStart.Z += (Up > 0.f ? 1.f : -1.f) * (HalfHeight + ClimbCheckLead);
	const FVector ProbeEnd = ProbeStart + ForwardVector * WallSearchDist;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WallClimbMove), false, Player);

	// 벽이 없으면(모서리 위, 벽 아래 끝) 기존처럼 움직인다. 올라가기·내려가기는 다른 어빌리티가 맡는다.
	FHitResult WallHit;
	if (!GetWorld()->LineTraceSingleByChannel(WallHit, ProbeStart, ProbeEnd, ECC_WallAttach, Params))
	{
		if (Up < 0.f) return true;

		// 위로 갈 때는 머리 높이(FindWall의 머리 트레이스와 같은 높이)에 아직 벽이 있을 때만 올라간다.
		// 벽 위 끝을 넘으면 여기서 멈추고 모서리 올라가기(LedgeClimb)에 맡긴다. 올라가기가 안 되는 곳에서 허공으로 계속 올라가지 않게 한다.
		FVector HeadStart = Player->GetActorLocation();
		HeadStart.Z += HalfHeight * 0.5f;
		FHitResult HeadHit;
		return GetWorld()->LineTraceSingleByChannel(HeadHit, HeadStart, HeadStart + ForwardVector * WallSearchDist, ECC_WallAttach, Params);
	}

	// 벽이 있으면 벽타기 벽이어야 이어서 움직인다
	FHitResult ClimbHit;
	return GetWorld()->LineTraceSingleByChannel(ClimbHit, ProbeStart, ProbeEnd, ECC_WallClimbing, Params);
}

bool UGAWallAttach::FindWall(const ACharacter* Player, FHitResult& OutHit, bool* bOutClimbable) const
{
	// 액터의 위치에서 액터의 전방으로 WallSearchDist 만큼 트레이스

	const float PlayerFacing = Player->GetActorForwardVector().X;
	const float SearchDir = PlayerFacing > 0 ? 1 : -1;
	const FVector ForwardVector(SearchDir, 0, 0);

	FHitResult BodyHit, HeadHit;
	FVector BodyStart = Player->GetActorLocation();
	FVector HeadStart = BodyStart;
	HeadStart.Z += Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.5f;
	FVector BodyEnd = BodyStart + ForwardVector * WallSearchDist;
	FVector HeadEnd = HeadStart + ForwardVector * WallSearchDist;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WallAttach), false, Player);

	if (!GetWorld()->LineTraceSingleByChannel(BodyHit, BodyStart, BodyEnd, ECC_WallAttach, Params))
	{
		// 몸통 높이가 창문·틈에 걸려 빗나가도 허리 아래에 벽이 있으면 매달린다
		const FVector LowStart = BodyStart - FVector(0.f, 0.f, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * LowProbeHeightRatio);
		if (!GetWorld()->LineTraceSingleByChannel(BodyHit, LowStart, LowStart + ForwardVector * WallSearchDist, ECC_WallAttach, Params))
		{
			return false;
		}
	}

	if (!GetWorld()->LineTraceSingleByChannel(HeadHit, HeadStart, HeadEnd, ECC_WallAttach, Params))
	{
		return false;
	}

	if (FMath::Abs(BodyHit.ImpactNormal.Z) > MaxWallNormalZ) return false;
	if (FMath::Abs(HeadHit.ImpactNormal.Z) > MaxWallNormalZ) return false;

	// 이미 파 묻힌채 시작한거면 안도이ㅣㅣ
	if (BodyHit.bStartPenetrating) return false;
	if (HeadHit.bStartPenetrating) return false;


	// Pawn과의 충돌은 제외
	const auto* BodyHitComponent = BodyHit.GetComponent();
	if (!IsValid(BodyHitComponent)) return false;
	if (BodyHitComponent->GetCollisionObjectType() == ECC_Pawn) return false;
	if (BodyHitComponent->GetCollisionObjectType() == ECC_Monster)
	{
		return false;
	}

	const auto* HeadHitComponent = HeadHit.GetComponent();
	if (!IsValid(HeadHitComponent)) return false;
	if (HeadHitComponent->GetCollisionObjectType() == ECC_Pawn) return false;
	if (HeadHitComponent->GetCollisionObjectType() == ECC_Monster)
	{
		return false;
	}

	// 머리와 몸통의 결과가 다른 곳인지 확인 (계단 형태인지 확인)
	if (FMath::Abs(BodyHit.Distance - HeadHit.Distance) > MaxWallDepthDiff) return false;

	// 충돌면이 탐색방향과 마주보고 있는지 확인
	const float FacingDot = FVector::DotProduct(ForwardVector, BodyHit.ImpactNormal);
	if (FacingDot >= 0)  return false;

	// 바닥이 너무 가까우면 못 매달림
	const FVector Feet = BodyStart - FVector(0.f, 0.f, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FHitResult GroundHit;
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, Feet, Feet - FVector(0.f, 0.f, MinHeightAboveGround), ECC_Visibility, Params)) return false;

	OutHit = BodyHit;

	// 해당 값을 요청한 경우에만 값 반환
	if (bOutClimbable)
	{
		FHitResult ClimbHit;
		*bOutClimbable = GetWorld()->LineTraceSingleByChannel(ClimbHit, HeadStart, HeadEnd, ECC_WallClimbing, Params);

#if ENABLE_DRAW_DEBUG
		if (bDrawDebugClimbable)
		{
			constexpr float Duration = 3.f;
			const FColor Color = *bOutClimbable ? FColor::Green : FColor::Red;

			// 트레이스 선 (맞았으면 맞은 지점까지만 초록)
			DrawDebugLine(GetWorld(), HeadStart, *bOutClimbable ? ClimbHit.ImpactPoint : HeadEnd, Color, false, Duration, 0, 1.5f);
			if (*bOutClimbable)
			{
				DrawDebugPoint(GetWorld(), ClimbHit.ImpactPoint, 12.f, FColor::Green, false, Duration);
				DrawDebugString(GetWorld(), ClimbHit.ImpactPoint + FVector(0.f, 0.f, 20.f),
					FString::Printf(TEXT("Climbable: %s"), *GetNameSafe(ClimbHit.GetComponent())), nullptr, FColor::Green, Duration);
			}
			else
			{
				DrawDebugString(GetWorld(), HeadEnd + FVector(0.f, 0.f, 20.f), TEXT("Not Climbable"), nullptr, FColor::Red, Duration);
			}

			// 비교용: 붙은 벽(WallAttach 채널) 지점
			DrawDebugPoint(GetWorld(), OutHit.ImpactPoint, 10.f, FColor::Yellow, false, Duration);
		}
#endif
	}

	return true;
}
