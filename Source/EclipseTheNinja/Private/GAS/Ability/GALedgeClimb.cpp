


#include "GAS/Ability/GALedgeClimb.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/PlayerCharacter.h"
#include "Character/PlayerCharacterCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveVector.h"
#include "DrawDebugHelpers.h"
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

	// 몽타주 재생
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

	// 런타임에 이동 경로 보정
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

	// 설 자리 높이는 윗면 + 반높이 + 1 이다
	ClearLedgeInPath(Player, Player->GetActorLocation(), StandLocation.Z - Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 1.f);

	UAbilityTask_ApplyRootMotionMoveToForce* MoveTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this, NAME_None, StandLocation, Duration, false, MOVE_Flying, false, PathCurve,
		ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.0f);
	MoveTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UGALedgeClimb::OnClimbFinished);
	MoveTask->OnTimedOut.AddDynamic(this, &UGALedgeClimb::OnClimbFinished);
	MoveTask->ReadyForActivation();
}

void UGALedgeClimb::ClearLedgeInPath(const ACharacter* Player, const FVector& Start, float LedgeTopZ)
{
	if (!IsValid(PathCurve)) return;

	const float Radius = Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Delta = StandLocation - Start;
	const float Horizontal = Delta.Size2D();
	if (Horizontal <= 1.f) return;

	// 이동 태스크는 [시작~끝 직선 + 곡선 오프셋(X: 진행 방향, Z: 위)]으로 움직인다. 같은 방식으로 경로를 샘플링한다.
	FRichCurve& ForwardCurve = PathCurve->FloatCurves[0];
	FRichCurve& UpCurve = PathCurve->FloatCurves[2];

	constexpr int32 SampleCount = 32;
	TArray<float> Forward, Height;
	Forward.SetNum(SampleCount + 1);
	Height.SetNum(SampleCount + 1);
	float MaxRiseStep = 0.f;

	// 이동 경로를 수직/수평 성분으로 구분해서 SampleCount + 1개 만큼 샘플링
	// 그 중에서 수직 거리가 가장 크게 변한 값을 저장
	for (int32 Index = 0; Index <= SampleCount; ++Index)
	{
		const float Fraction = static_cast<float>(Index) / SampleCount;
		Forward[Index] = ForwardCurve.Eval(Fraction);
		Height[Index] = Start.Z + Delta.Z * Fraction + UpCurve.Eval(Fraction);
		if (Index > 0) MaxRiseStep = FMath::Max(MaxRiseStep, Height[Index] - Height[Index - 1]);
	}

	// 1. 시작 자리(벽에 붙은 자리)에서 안쪽으로 들어간 거리만큼 캡슐 아래 반구가 윗면 위에 있어야 한다
	constexpr float Clearance = 0.5f;
	bool bChanged = false;
	TArray<float> Cleared = Height;
	for (int32 Index = 0; Index <= SampleCount; ++Index)
	{
		const float Fraction = static_cast<float>(Index) / SampleCount;
		// 해당 커브가 수평으로 진행한 비율 x 실제 수평 거리 = 안쪽으로 들어간 거리
		const float Inward = Horizontal * Fraction + Forward[Index];
		if (Inward <= 0.f) continue;

		const float Overlap = FMath::Min(Inward, Radius);
		const float Required = LedgeTopZ + HalfHeight - Radius + FMath::Sqrt(FMath::Max(0.f, Radius * Radius - FMath::Square(Radius - Overlap))) + Clearance;
		if (Cleared[Index] < Required)
		{
			Cleared[Index] = FMath::Min(Required, StandLocation.Z);
			bChanged = true;
		}
	}
	if (!bChanged) return;

	// 2. 갑자기 솟지 않도록 앞 구간에서 미리 올라가게 한다 (원래 경로에서 가장 빠르게 오르던 속도까지)
	MaxRiseStep = FMath::Max(MaxRiseStep, 1.f);
	for (int32 Index = SampleCount - 1; Index >= 1; --Index)
	{
		Cleared[Index] = FMath::Max(Cleared[Index], Cleared[Index + 1] - MaxRiseStep);
	}

	// 3. 곡선을 다시 만든다. 직선 보간이라 키 사이에서 모서리 쪽으로 넘치지 않는다.
	ForwardCurve.Reset();
	UpCurve.Reset();
	for (int32 Index = 0; Index <= SampleCount; ++Index)
	{
		const float Fraction = static_cast<float>(Index) / SampleCount;
		ForwardCurve.SetKeyInterpMode(ForwardCurve.AddKey(Fraction, Forward[Index]), RCIM_Linear);
		UpCurve.SetKeyInterpMode(UpCurve.AddKey(Fraction, Cleared[Index] - Start.Z - Delta.Z * Fraction), RCIM_Linear);
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugClimb)
	{
		const FVector Dir = Delta.GetSafeNormal2D();
		for (int32 Index = 0; Index <= SampleCount; ++Index)
		{
			const float Fraction = static_cast<float>(Index) / SampleCount;
			const FVector Base = Start + Dir * (Horizontal * Fraction + Forward[Index]);
			DrawDebugPoint(GetWorld(), FVector(Base.X, Base.Y, Height[Index]), 5.f, FColor::Red, false, 5.f);
			DrawDebugPoint(GetWorld(), FVector(Base.X, Base.Y, Cleared[Index]), 5.f, FColor::Green, false, 5.f);
		}
	}
#endif
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
	const FVector LocationBeforeSnap = Player->GetActorLocation();

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

	// 설 자리에 걸을 수 있는 바닥이 있으면 바로 걷기로 넘긴다 (낙하로 넘기면 애님 BP가 착지 동작을 재생한다).
	// 걷기로 바뀔 때 CMC가 바닥 높이에 캡슐을 맞춘다. 바닥이 없으면 낙하로 넘겨 착지하게 한다.
	FFindFloorResult FloorResult;
	if (bStand)
	{
		Movement->FindFloor(Player->GetActorLocation(), FloorResult, false);
	}
	Movement->SetMovementMode(FloorResult.IsWalkableFloor() ? MOVE_Walking : MOVE_Falling);

	// 카메라는 캐릭터를 바로 따라가므로, 설 자리에 맞추고 바닥에 붙이며 캡슐이 순간 이동한 만큼 튀지 않게 부드럽게 따라오게 한다
	if (const APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		if (APlayerCharacterCameraManager* CameraManager = Cast<APlayerCharacterCameraManager>(PC->PlayerCameraManager))
		{
			CameraManager->SmoothTeleport(Player->GetActorLocation() - LocationBeforeSnap);
		}
	}
}

void UGALedgeClimb::DrawClimbDebug(const ACharacter* Player, float LifeTime) const
{
#if ENABLE_DRAW_DEBUG
	if (!bDrawDebugClimb || !IsValid(Player)) return;

	FVector TestStand;
	float TestFacing = 0.f;
	FindClimb(Player, TestStand, TestFacing, FMath::Max(0.f, LifeTime));
#endif
}

bool UGALedgeClimb::FindClimb(const ACharacter* Player, FVector& OutStandLocation, float& OutFacing, float DebugLifeTime) const
{
	const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Location = Player->GetActorLocation();

	// 매달린 동안에는 벽을 바라보고 있다고 가정한다.
	OutFacing = Player->GetActorForwardVector().X >= 0.f ? 1.f : -1.f;
	const FVector Dir(OutFacing, 0.f, 0.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LedgeClimb), false, Player);
	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse = Capsule->GetCollisionResponseToChannels();
	const ECollisionChannel PawnChannel = Capsule->GetCollisionObjectType();

	// 디버그 표시 (DebugLifeTime >= 0일 때만): 실패하면 이유를 캐릭터 위에 빨간 글씨로, 성공하면 초록 글씨로 띄운다
	const bool bDebug = DebugLifeTime >= 0.f;
	auto DebugResult = [&](const TCHAR* Reason, bool bSuccess)
	{
#if ENABLE_DRAW_DEBUG
		if (!bDebug) return;
		DrawDebugString(GetWorld(), Location + FVector(0.f, 0.f, HalfHeight + 30.f),
			FString::Printf(TEXT("LedgeClimb: %s"), Reason), nullptr, bSuccess ? FColor::Green : FColor::Red, DebugLifeTime, true);
#endif
	};

	// 1. 벽면 위치: 캡슐 중심에서 벽 쪽으로 [반지름(캡슐 표면까지) + WallTraceMargin(표면 앞 여유)]만큼 쏜다
	FHitResult WallHit;
	const FVector WallEnd = Location + Dir * (Radius + WallTraceMargin);
	const bool bWallHit = GetWorld()->LineTraceSingleByChannel(WallHit, Location, WallEnd, PawnChannel, Params, ResponseParams);
#if ENABLE_DRAW_DEBUG
	if (bDebug)
	{
		DrawDebugLine(GetWorld(), Location, bWallHit ? WallHit.ImpactPoint : WallEnd, bWallHit ? FColor::Green : FColor::Red, false, DebugLifeTime, 0, 1.5f);
		if (bWallHit) DrawDebugPoint(GetWorld(), WallHit.ImpactPoint, 10.f, FColor::Green, false, DebugLifeTime);
	}
#endif
	if (!bWallHit)
	{
		DebugResult(TEXT("No wall"), false);
		return false;
	}
	const float WallX = WallHit.ImpactPoint.X;

	// 벽 방향으로 ClimbInset 들어간다.( 올라갔을 때 플레이어가 모서리로부터 얼마나 떨어져 있을지) 
	const float StandX = WallX + OutFacing * (Radius + ClimbInset);
	FHitResult TopHit;

	// 캐릭터가 설 위치에서 아래로 라인 트레이스 
	// 올라갈 수 있는 최대거리 위에서 아래로 라인 트레이스
	// 해당 위치에 발을 디딜 수 있는 땅이 있는지
	const FVector TopStart(StandX, Location.Y, Location.Z + MaxReachHeight);
	const FVector TopEnd(StandX, Location.Y, Location.Z);
	const bool bTopHit = GetWorld()->LineTraceSingleByChannel(TopHit, TopStart, TopEnd, PawnChannel, Params, ResponseParams);
#if ENABLE_DRAW_DEBUG
	if (bDebug)
	{
		// 올라갈 수 있는 범위: 벽면 ~ 설 자리 너머, 캡슐 중심 ~ MaxReachHeight. 모서리 윗면이 이 상자 안에 있어야 한다.
		const FVector ReachMin(FMath::Min(WallX, StandX + OutFacing * Radius), Location.Y - 5.f, Location.Z);
		const FVector ReachMax(FMath::Max(WallX, StandX + OutFacing * Radius), Location.Y + 5.f, TopStart.Z);
		const FBox ReachBox(ReachMin, ReachMax);
		DrawDebugSolidBox(GetWorld(), ReachBox, FColor(255, 255, 0, 40), FTransform::Identity, false, DebugLifeTime);
		DrawDebugBox(GetWorld(), ReachBox.GetCenter(), ReachBox.GetExtent(), FColor::Yellow, false, DebugLifeTime, 0, 1.f);
		DrawDebugString(GetWorld(), FVector(StandX, Location.Y, TopStart.Z + 10.f), FString::Printf(TEXT("MaxReach +%.0f"), MaxReachHeight), nullptr, FColor::Yellow, DebugLifeTime, true);

		// 윗면 트레이스
		DrawDebugLine(GetWorld(), TopStart, bTopHit ? TopHit.ImpactPoint : TopEnd, FColor::Cyan, false, DebugLifeTime, 0, 1.5f);
		if (bTopHit)
		{
			const bool bWalkable = !TopHit.bStartPenetrating && TopHit.ImpactNormal.Z >= 0.7f;
			const FColor TopColor = bWalkable ? FColor::Green : FColor::Red;
			DrawDebugPoint(GetWorld(), TopHit.ImpactPoint, 12.f, TopColor, false, DebugLifeTime);
			DrawDebugDirectionalArrow(GetWorld(), TopHit.ImpactPoint, TopHit.ImpactPoint + TopHit.ImpactNormal * 40.f, 10.f, TopColor, false, DebugLifeTime, 0, 1.5f);
			DrawDebugString(GetWorld(), TopHit.ImpactPoint + FVector(0.f, 0.f, 15.f),
				FString::Printf(TEXT("Ledge +%.0f"), TopHit.ImpactPoint.Z - Location.Z), nullptr, TopColor, DebugLifeTime, true);
		}
	}
#endif
	if (!bTopHit)
	{
		// 얇은 벽등 올라갈 수 있는 모서리가 아님
		DebugResult(TEXT("No ledge top within reach"), false);
		return false;
	}
	if (TopHit.bStartPenetrating)
	{
		// 이미 파묻힌 상태 모서리까지 너무 먼 상태ㅔ
		DebugResult(TEXT("Top trace starts inside geometry (wall too tall)"), false);
		return false;
	}
	if (TopHit.ImpactNormal.Z < 0.7f)
	{
		// 벽이 너무 가파름
		DebugResult(TEXT("Ledge top too steep"), false);
		return false;
	}

	// 서있을 위치 기록
	OutStandLocation = FVector(StandX, Location.Y, TopHit.ImpactPoint.Z + HalfHeight + 1.f);

	// 3. 올라가는 경로(위로 → 안쪽으로)와 설 자리가 막혀 있지 않아야 한다
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(FMath::Max(1.f, Radius - 2.f), FMath::Max(1.f, HalfHeight - 2.f));
	const FVector AboveLedge(Location.X, Location.Y, OutStandLocation.Z + 2.f);

	FHitResult PathHit;

	// 캐릭터 머리 위로 캐릭터의 캡슐과 동일한 캡슐을 ㄱ형태로 스윕하여 진짜 모서리가 맞는지 판단
	// 캐릭터의 위쪽이 막혀있는지
	const bool bUpBlocked = GetWorld()->SweepSingleByChannel(PathHit, Location, AboveLedge, FQuat::Identity, PawnChannel, Shape, Params, ResponseParams);
	const FHitResult UpHit = PathHit;

	// 방금 지점에서 캐릭터가 서있을 위치까지 갈 수 있는지
	const bool bInBlocked = !bUpBlocked && GetWorld()->SweepSingleByChannel(PathHit, AboveLedge, OutStandLocation + FVector(0.f, 0.f, 2.f), FQuat::Identity, PawnChannel, Shape, Params, ResponseParams);
#if ENABLE_DRAW_DEBUG
	if (bDebug)
	{
		const float ShapeRadius = Shape.GetCapsuleRadius();
		const float ShapeHalfHeight = Shape.GetCapsuleHalfHeight();
		const FColor UpColor = bUpBlocked ? FColor::Red : FColor::Green;
		DrawDebugLine(GetWorld(), Location, AboveLedge, UpColor, false, DebugLifeTime, 0, 2.f);
		DrawDebugCapsule(GetWorld(), AboveLedge, ShapeHalfHeight, ShapeRadius, FQuat::Identity, UpColor, false, DebugLifeTime);
		if (bUpBlocked) DrawDebugPoint(GetWorld(), UpHit.ImpactPoint, 12.f, FColor::Red, false, DebugLifeTime);

		const FColor InColor = bUpBlocked ? FColor::Silver : (bInBlocked ? FColor::Red : FColor::Green);
		DrawDebugLine(GetWorld(), AboveLedge, OutStandLocation, InColor, false, DebugLifeTime, 0, 2.f);
		DrawDebugCapsule(GetWorld(), OutStandLocation, HalfHeight, Radius, FQuat::Identity, InColor, false, DebugLifeTime, 0, 2.f);
		if (bInBlocked) DrawDebugPoint(GetWorld(), PathHit.ImpactPoint, 12.f, FColor::Red, false, DebugLifeTime);
	}
#endif
	if (bUpBlocked)
	{
		DebugResult(TEXT("Upward path blocked"), false);
		return false;
	}
	if (bInBlocked)
	{
		DebugResult(TEXT("Stand spot / inward path blocked"), false);
		return false;
	}

	DebugResult(*FString::Printf(TEXT("Can climb (height +%.0f)"), TopHit.ImpactPoint.Z - Location.Z), true);
	return true;
}
