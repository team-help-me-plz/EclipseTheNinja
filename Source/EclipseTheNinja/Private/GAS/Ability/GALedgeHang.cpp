


#include "GAS/Ability/GALedgeHang.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/PlayerCharacter.h"
#include "Character/PlayerCharacterCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveVector.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RootMotionSource.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Ability/GAWallAttach.h"
#include "GAS/StatAttributeSet.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LedgeHang, "Ability.Movement.LedgeHang");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LedgeHanging, "State.Movement.LedgeHanging");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LedgeHold, "State.Movement.LedgeHold");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LedgeHangPose, "State.Movement.LedgeHangPose");
static const ECollisionChannel ECC_WallAttach = ECC_GameTraceChannel2;

UGALedgeHang::UGALedgeHang()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_LedgeHang);
	SetAssetTags(Tags);

	// 내려가는 동안에는 공격·암살이 나가지 않도록 공격중 태그를 붙인다.
	// 매달리기(GAWallAttach)는 공격중 태그에 막히므로, 이 어빌리티를 끝낸 뒤에 활성화한다.
	ActivationOwnedTags.AddTag(TAG_LedgeHanging);
	ActivationOwnedTags.AddTag(UGABaseAttack::GetAttackingTag());
	ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());
	ActivationBlockedTags.AddTag(UGAWallAttach::GetWallAttachingTag());
}

FGameplayTag UGALedgeHang::GetLedgeHangingTag()
{
	return TAG_LedgeHanging;
}

FGameplayTag UGALedgeHang::GetLedgeHoldTag()
{
	return TAG_LedgeHold;
}

FGameplayTag UGALedgeHang::GetLedgeHangPoseTag()
{
	return TAG_LedgeHangPose;
}
FVector UGALedgeHang::GetMontageRootMotion(const UAnimMontage* Montage, int32 SegmentIndex)
{
	if (!IsValid(Montage) || Montage->SlotAnimTracks.Num() == 0) return FVector::ZeroVector;
	const TArray<FAnimSegment>& Segments = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments;
	const int32 Index = SegmentIndex == INDEX_NONE ? Segments.Num() - 1 : SegmentIndex;
	if (!Segments.IsValidIndex(Index)) return FVector::ZeroVector;

	const FAnimExtractContext Context;
	const float Start = Segments[Index].StartPos;
	return Montage->ExtractRootMotionFromTrackRange(Start, Start + Segments[Index].GetLength(), Context).GetTranslation();
}

bool UGALedgeHang::BuildRootMotionPath(const UAnimMontage* Montage, const FVector& Start, const FVector& End,
	UCurveVector* Curve, float& OutDuration)
{
	if (!IsValid(Montage) || Montage->SlotAnimTracks.Num() == 0) return false;
	const TArray<FAnimSegment>& Segments = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments;
	if (Segments.Num() == 0) return false;

	// 길이는 첫 구간에서 구한다 (구간을 바꾼 뒤 저장된 몽타주 길이가 갱신되지 않을 수 있다)
	OutDuration = Segments[0].StartPos + Segments[0].GetLength();

	// 수평·수직 이동이 모두 있어야 모서리를 피하는 궤적이 된다. 없으면 직선 대각선 이동이 되어 모서리에 걸린다.
	const FVector AnimTotal = GetMontageRootMotion(Montage, 0);
	if (AnimTotal.Size2D() <= 1.0f || FMath::Abs(AnimTotal.Z) <= 1.0f) return false;

	return BuildRootMotionPathInRange(Montage, 0.f, OutDuration, Start, End, Curve);
}

bool UGALedgeHang::BuildRootMotionPathInRange(const UAnimMontage* Montage, float StartTime, float EndTime,
	const FVector& Start, const FVector& End, UCurveVector* Curve)
{
	const float Duration = EndTime - StartTime;
	if (!IsValid(Montage) || !IsValid(Curve) || Duration <= KINDA_SMALL_NUMBER) return false;

	const FAnimExtractContext Context;
	const FVector AnimTotal = Montage->ExtractRootMotionFromTrackRange(StartTime, EndTime, Context).GetTranslation();
	const FVector AnimDirection = AnimTotal.GetSafeNormal2D();
	const float AnimHorizontal = AnimTotal.Size2D();
	const float AnimVertical = AnimTotal.Z;
	if (AnimHorizontal <= 1.0f && FMath::Abs(AnimVertical) <= 1.0f) return false;

	// 이동 태스크는 시작~끝을 일정한 속도로 잇고 이 곡선을 더한다(X: 진행 방향, Z: 위).
	// 몽타주 루트 모션이 진행한 비율을 따라가도록 직선과의 차이를 넣는다. 해당 축 이동이 거의 없으면 직선 진행으로 둔다.
	FRichCurve& ForwardCurve = Curve->FloatCurves[0];
	FRichCurve& UpCurve = Curve->FloatCurves[2];
	ForwardCurve.Reset();
	UpCurve.Reset();

	const FVector WorldDelta = End - Start;
	const float WorldHorizontal = WorldDelta.Size2D();
	const float WorldVertical = WorldDelta.Z;

	constexpr int32 SampleCount = 16;
	for (int32 Index = 0; Index <= SampleCount; ++Index)
	{
		const float Fraction = static_cast<float>(Index) / SampleCount;
		const FVector AnimPartial = Montage->ExtractRootMotionFromTrackRange(StartTime, StartTime + Duration * Fraction, Context).GetTranslation();

		const float HorizontalProgress = AnimHorizontal > 1.0f
			? FVector::DotProduct(FVector(AnimPartial.X, AnimPartial.Y, 0.0f), AnimDirection) / AnimHorizontal : Fraction;
		const float VerticalProgress = FMath::Abs(AnimVertical) > 1.0f ? AnimPartial.Z / AnimVertical : Fraction;

		ForwardCurve.SetKeyInterpMode(ForwardCurve.AddKey(Fraction, (HorizontalProgress - Fraction) * WorldHorizontal), RCIM_Cubic);
		UpCurve.SetKeyInterpMode(UpCurve.AddKey(Fraction, (VerticalProgress - Fraction) * WorldVertical), RCIM_Cubic);
	}
	ForwardCurve.AutoSetTangents();
	UpCurve.AutoSetTangents();
	return true;
}

bool UGALedgeHang::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	if (!ActorInfo) return false;

	const APlayerCharacter* Player = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Player)) return false;

	// 플레이어가 죽은 경우 실행 불가
	const UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(SourceASC)) return false;
	if (SourceASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f) return false;

	// 서 있을 때만 모서리에서 내려간다
	if (!Player->GetCharacterMovement()->IsMovingOnGround()) return false;

	// 내려간 뒤 넘겨줄 매달리기 어빌리티가 없으면 모서리 밖으로 떨어지기만 하므로 실행하지 않는다
	const FGameplayTag WallAttachTag = UGAWallAttach::GetWallAttachAbilityTag();
	const bool bHasWallAttach = SourceASC->GetActivatableAbilities().ContainsByPredicate(
		[&WallAttachTag](const FGameplayAbilitySpec& Spec)
		{
			return Spec.Ability && Spec.Ability->GetAssetTags().HasTagExact(WallAttachTag);
		});
	if (!bHasWallAttach) return false;

	FLedgePlan TestPlan;
	return FindLedge(Player, TestPlan);
}

void UGALedgeHang::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	bMovementChanged = false;
	bReachedHang = false;
	bHangPoseTagAdded = false;

	APlayerCharacter* Player = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Player) || !FindLedge(Player, Plan))
	{
		OnInterrupted();
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		OnInterrupted();
		return;
	}

	// 이동은 루트 모션 태스크가 맡으므로 중력 없는 Flying으로 바꾼다. 종료 시 매달리기나 낙하로 넘긴다.
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Flying);
	bMovementChanged = true;

	// 벽(바깥 방향의 반대)을 바라본다. 커서·이동 입력 회전도 이 방향으로 고정된다.
	Player->SetAssassinationFacing(Plan.OutDir > 0.f ? 180.f : 0.f);

	LoadedDropMontage = DropMontage.LoadSynchronous();
	if (IsValid(LoadedDropMontage))
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, LoadedDropMontage, 1.0f, NAME_None, true);
		MontageTask->ReadyForActivation();

		// 위치는 코드가 맡으므로 몽타주 루트 모션은 끈다 (몽타주는 어빌리티가 끝날 때 함께 멈춘다)
		UAnimInstance* AnimInstance = Player->GetMesh() ? Player->GetMesh()->GetAnimInstance() : nullptr;
		if (FAnimMontageInstance* MontageInstance = AnimInstance ? AnimInstance->GetActiveInstanceForMontage(LoadedDropMontage) : nullptr)
		{
			MontageInstance->PushDisableRootMotion();
		}

		// 몽타주가 다 섞여 들어와 아래 자세를 가린 뒤에 애님 BP를 매달리기 상태로 넘긴다.
		// 그 전에 넘기면 섞여 들어오는 동안 매달리기 자세가 비쳐 보인다.
		const float BlendInTime = LoadedDropMontage->BlendIn.GetBlendTime();
		if (BlendInTime <= KINDA_SMALL_NUMBER)
		{
			OnHangPoseReady();
		}
		else
		{
			UAbilityTask_WaitDelay* PoseTask = UAbilityTask_WaitDelay::WaitDelay(this, BlendInTime);
			PoseTask->OnFinish.AddDynamic(this, &UGALedgeHang::OnHangPoseReady);
			PoseTask->ReadyForActivation();
		}
	}

	if (!IsValid(PathCurve))
	{
		PathCurve = NewObject<UCurveVector>(this);
	}

	// 구간이 둘이면 [제자리에서 앉기 + 아래로 내려가기]: 첫 구간 동안 제자리에 있고, 둘째 구간 동안 내려간다.
	bTwoSectionDrop = false;
	const TArray<FAnimSegment>* Segments = (IsValid(LoadedDropMontage) && LoadedDropMontage->SlotAnimTracks.Num() > 0)
		? &LoadedDropMontage->SlotAnimTracks[0].AnimTrack.AnimSegments : nullptr;
	if (Segments && Segments->Num() >= 2)
	{
		bTwoSectionDrop = true;
		SectionTime = (*Segments)[1].StartPos;
		DropEndTime = (*Segments)[1].StartPos + (*Segments)[1].GetLength();
		CrossfadeTime = FMath::Clamp(LoadedDropMontage->BlendIn.GetBlendTime(), 0.f, SectionTime);

		// 1구간: 앉는 동안 제자리에 둔다 (첫 구간에 루트 모션이 있으면 그 모양대로 움직이고 같은 자리로 돌아온다)
		const bool bHasPath = BuildRootMotionPathInRange(LoadedDropMontage, 0.f, SectionTime,
			Player->GetActorLocation(), Plan.EdgeStandLocation, PathCurve);
		UAbilityTask_ApplyRootMotionMoveToForce* StepTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
			this, NAME_None, Plan.EdgeStandLocation, FMath::Max(0.05f, SectionTime), false, MOVE_Flying, false,
			bHasPath ? PathCurve.Get() : nullptr, ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.0f);
		StepTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UGALedgeHang::OnStepOutFinished);
		StepTask->OnTimedOut.AddDynamic(this, &UGALedgeHang::OnStepOutFinished);
		StepTask->ReadyForActivation();

		// 두 구간 경계의 자세 차이는 섞는 시간만큼 먼저 둘째 구간을 다시 재생해 넘긴다
		const float CrossfadeStart = SectionTime - CrossfadeTime;
		if (CrossfadeStart <= KINDA_SMALL_NUMBER)
		{
			OnSectionCrossfade();
		}
		else
		{
			UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, CrossfadeStart);
			WaitTask->OnFinish.AddDynamic(this, &UGALedgeHang::OnSectionCrossfade);
			WaitTask->ReadyForActivation();
		}
		return;
	}

	// 구간이 하나면 몽타주 루트 모션 궤적을 따라 한 번에 매달림 위치까지 이동한다.
	// 올라가기를 거꾸로 재생하는 몽타주면 먼저 뒤로 빠진 뒤 내려가므로 모서리에 걸리지 않는다.
	float Duration = 0.f;
	if (BuildRootMotionPath(LoadedDropMontage, Player->GetActorLocation(), Plan.DropEndLocation, PathCurve, Duration))
	{
		UAbilityTask_ApplyRootMotionMoveToForce* MoveTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
			this, NAME_None, Plan.DropEndLocation, Duration, false, MOVE_Flying, false, PathCurve,
			ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.0f);
		MoveTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UGALedgeHang::OnDropFinished);
		MoveTask->OnTimedOut.AddDynamic(this, &UGALedgeHang::OnDropFinished);
		MoveTask->ReadyForActivation();
		return;
	}

	// 몽타주나 루트 모션이 없으면 두 구간으로 이동한다. 1구간: 모서리 밖으로 한 걸음
	UAbilityTask_ApplyRootMotionMoveToForce* StepTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this, NAME_None, Plan.StepOutLocation, StepOutTime, false, MOVE_Flying, false, nullptr,
		ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.0f);
	StepTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UGALedgeHang::OnStepOutFinished);
	StepTask->OnTimedOut.AddDynamic(this, &UGALedgeHang::OnStepOutFinished);
	StepTask->ReadyForActivation();
}

void UGALedgeHang::OnSectionCrossfade()
{
	if (!IsActive() || !IsValid(LoadedDropMontage)) return;

	ACharacter* Player = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Player)) return;

	// 같은 몽타주를 둘째 구간부터 새로 재생하면 이전 재생은 Blend In 시간 동안 섞여 빠진다
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, LoadedDropMontage, 1.0f, NAME_None, true, 1.0f, SectionTime);
	MontageTask->ReadyForActivation();

	UAnimInstance* AnimInstance = Player->GetMesh() ? Player->GetMesh()->GetAnimInstance() : nullptr;
	if (FAnimMontageInstance* MontageInstance = AnimInstance ? AnimInstance->GetActiveInstanceForMontage(LoadedDropMontage) : nullptr)
	{
		MontageInstance->PushDisableRootMotion();
	}
}

void UGALedgeHang::OnStepOutFinished()
{
	if (!IsActive()) return;

	// 2구간: 매달림 위치까지 내려간다
	float Duration = DropTime;
	UCurveVector* Curve = nullptr;
	if (bTwoSectionDrop)
	{
		// 둘째 구간은 섞는 시간만큼 먼저 시작했으므로 그만큼 앞선 지점부터 몽타주 끝까지 따라간다
		const float StartTime = SectionTime + CrossfadeTime;
		Duration = FMath::Max(0.05f, DropEndTime - StartTime);
		const AActor* Avatar = GetAvatarActorFromActorInfo();
		if (IsValid(Avatar) && BuildRootMotionPathInRange(LoadedDropMontage, StartTime, DropEndTime,
			Avatar->GetActorLocation(), Plan.DropEndLocation, PathCurve))
		{
			Curve = PathCurve;
		}
	}

	UAbilityTask_ApplyRootMotionMoveToForce* DropTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this, NAME_None, Plan.DropEndLocation, Duration, false, MOVE_Flying, false, Curve,
		ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.0f);
	DropTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UGALedgeHang::OnDropFinished);
	DropTask->OnTimedOut.AddDynamic(this, &UGALedgeHang::OnDropFinished);
	DropTask->ReadyForActivation();
}

void UGALedgeHang::OnHangPoseReady()
{
	if (!IsActive() || bHangPoseTagAdded) return;

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(TAG_LedgeHangPose);
		bHangPoseTagAdded = true;
	}
}

void UGALedgeHang::OnDropFinished()
{
	if (!IsActive()) return;

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	// 중간에 막혀 매달림 위치에 닿지 못했으면 그냥 떨어진다
	bReachedHang = IsValid(Avatar) && FVector::DistSquared(Avatar->GetActorLocation(), Plan.DropEndLocation) <= FMath::Square(20.0f);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, !bReachedHang);
}

void UGALedgeHang::OnInterrupted()
{
	if (!IsActive()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

void UGALedgeHang::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	APlayerCharacter* Player = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const bool bHang = bReachedHang && !bWasCancelled;
	const bool bRestoreMovement = bMovementChanged;
	bReachedHang = false;
	bMovementChanged = false;

	// 애님 BP는 틱에서 태그를 읽으므로, 같은 프레임 안에서 매달리기 태그로 바로 이어진다
	if (bHangPoseTagAdded && IsValid(ASC))
	{
		ASC->RemoveLooseGameplayTag(TAG_LedgeHangPose);
	}
	bHangPoseTagAdded = false;

	// 매달리기 Idle로 넘어갈 때는 몽타주 기본 Blend Out 대신 HangBlendOutTime 동안 천천히 섞여 빠지게 한다.
	// 먼저 멈춰 두면 아래 Super의 몽타주 태스크는 이미 빠지는 중인 몽타주를 다시 멈추지 않는다.
	if (bHang && IsValid(LoadedDropMontage) && IsValid(Player) && Player->GetMesh())
	{
		if (UAnimInstance* AnimInstance = Player->GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(HangBlendOutTime, LoadedDropMontage);
		}
	}

	// 공격중 태그가 빠져야 매달리기가 활성화되므로 먼저 끝낸다. 내려가기 몽타주도 여기서 섞여 빠진다.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (!IsValid(Player) || !bRestoreMovement) return;

	// 내려가는 동안 메시는 기본 위치에 둔다. 내려가기는 매달림 위치보다 메시 높이 오프셋만큼 위에서 끝나므로,
	// 캡슐을 매달림 위치로 내리면서 메시를 그만큼 바로 올리면 화면상 메시는 그대로이고 매달리기 Idle(+오프셋)로 이어진다.
	bool bAttached = false;
	if (bHang && IsValid(ASC))
	{
		// 매달리기는 바라보는 방향으로 벽을 찾으므로 위치와 방향을 다시 맞춘 뒤 넘긴다.
		const FVector LocationBeforeHandoff = Player->GetActorLocation();
		Player->SetActorLocation(Plan.HangLocation);
		Player->SetAssassinationFacing(Plan.OutDir > 0.f ? 180.f : 0.f);

		const ACharacter* DefaultCharacter = Player->GetClass()->GetDefaultObject<ACharacter>();
		const bool bRaiseMesh = !FMath::IsNearlyZero(Plan.MeshZOffset) && Player->GetMesh() && DefaultCharacter && DefaultCharacter->GetMesh();
		if (bRaiseMesh)
		{
			Player->GetMesh()->SetRelativeLocation(DefaultCharacter->GetMesh()->GetRelativeLocation() + FVector(0.f, 0.f, Plan.MeshZOffset));
		}

		// 내려가기 끝 자세를 그대로 붙잡아 두면 매달리기 자세로 바뀌며 튀지 않는다.
		// 표시 태그로 매달리기에 알려 메시 오프셋을 따로 주지 않게 한다.
		const bool bHolding = PlayHoldPose(Player);
		if (bHolding)
		{
			ASC->AddLooseGameplayTag(TAG_LedgeHold);
		}

		bAttached = ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(UGAWallAttach::GetWallAttachAbilityTag()));

		// 매달리지 못하면 내려가기가 끝난 자리와 메시 기본 위치로 돌려 그대로 떨어진다
		if (bRaiseMesh && !bAttached)
		{
			Player->SetActorLocation(Plan.DropEndLocation);
			Player->GetMesh()->SetRelativeLocation(DefaultCharacter->GetMesh()->GetRelativeLocation());
		}

		// 카메라는 캐릭터를 바로 따라가므로, 캡슐이 내려간 만큼 튀지 않게 부드럽게 따라오게 한다
		if (const APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		{
			if (APlayerCharacterCameraManager* CameraManager = Cast<APlayerCharacterCameraManager>(PC->PlayerCameraManager))
			{
				CameraManager->SmoothTeleport(Player->GetActorLocation() - LocationBeforeHandoff);
			}
		}

		if (bHolding && !bAttached)
		{
			ASC->RemoveLooseGameplayTag(TAG_LedgeHold);
			if (UAnimInstance* AnimInstance = Player->GetMesh() ? Player->GetMesh()->GetAnimInstance() : nullptr)
			{
				AnimInstance->Montage_Stop(0.2f, HoldMontage.Get());
			}
		}
	}

	if (!bAttached)
	{
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
}

bool UGALedgeHang::PlayHoldPose(ACharacter* Player) const
{
	UAnimMontage* Montage = HoldMontage.LoadSynchronous();
	UAnimInstance* AnimInstance = Player->GetMesh() ? Player->GetMesh()->GetAnimInstance() : nullptr;
	if (!IsValid(Montage) || !AnimInstance) return false;

	// 내려가기 몽타주는 이미 섞여 빠지는 중이므로 다른 몽타주는 멈추지 않는다
	if (AnimInstance->Montage_Play(Montage, 1.0f, EMontagePlayReturnType::MontageLength, 0.f, false) <= 0.f) return false;
	AnimInstance->Montage_Pause(Montage);
	return true;
}

float UGALedgeHang::GetHangMeshZOffset(const ACharacter* Player) const
{
	// 정지 자세를 쓰면 매달리기가 메시 오프셋을 주지 않는다
	if (!HoldMontage.IsNull()) return 0.f;

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player);
	if (!IsValid(ASC)) return 0.f;

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (const UGAWallAttach* WallAttach = Cast<UGAWallAttach>(Spec.Ability))
		{
			return WallAttach->GetHangMeshOffset().Z;
		}
	}
	return 0.f;
}

float UGALedgeHang::ResolveHangDepth(float CapsuleHalfHeight) const
{
	// 몽타주가 끝났을 때(모서리 위에 선 자세) 캡슐 중심은 윗면 + 반높이이고,
	// 루트 모션이 내려간 높이만큼 아래가 매달린 캡슐 중심이다.
	if (bHangDepthFromMontage)
	{
		// 아래로 내려가는 구간은 마지막 구간이다 (두 구간이면 앞 구간은 모서리까지 물러나기)
		const float Drop = FMath::Abs(GetMontageRootMotion(DropMontage.LoadSynchronous(), INDEX_NONE).Z);
		if (Drop > 1.0f)
		{
			// 매달리기 판정의 머리 높이 트레이스가 벽에 닿을 만큼은 내려가야 한다
			// 올라가기가 서는 높이(윗면 + 반높이 + 1)에서 루트 모션만큼 내려간 곳이다
			return FMath::Max(Drop - CapsuleHalfHeight - 1.f, CapsuleHalfHeight * 0.5f + 15.f);
		}
	}
	return HangDepth;
}

bool UGALedgeHang::FindLedge(const ACharacter* Player, FLedgePlan& OutPlan) const
{
	const float Facing = Player->GetActorForwardVector().X >= 0.f ? 1.f : -1.f;
	return FindLedgeInDirection(Player, Facing, OutPlan) || FindLedgeInDirection(Player, -Facing, OutPlan);
}

bool UGALedgeHang::FindLedgeInDirection(const ACharacter* Player, float OutDir, FLedgePlan& OutPlan) const
{
	const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Location = Player->GetActorLocation();
	const float FeetZ = Location.Z - HalfHeight;
	const FVector Dir(OutDir, 0.f, 0.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LedgeHang), false, Player);
	// 바닥과 이동 경로는 캐릭터 캡슐이 실제로 막히는 대상으로 확인한다 (통과 발판 설정 등 반영)
	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse = Capsule->GetCollisionResponseToChannels();
	const ECollisionChannel PawnChannel = Capsule->GetCollisionObjectType();

	// 1. 모서리: 캡슐 앞 EdgeSearchDistance 지점에 바닥이 없어야 한다
	const FVector Probe = Location + Dir * (Radius + EdgeSearchDistance);
	FHitResult FloorHit;
	if (GetWorld()->LineTraceSingleByChannel(FloorHit, FVector(Probe.X, Probe.Y, FeetZ + 10.f),
		FVector(Probe.X, Probe.Y, FeetZ - 30.f), PawnChannel, Params, ResponseParams))
	{
		return false;
	}

	// 2. 모서리 아래 벽면: 바깥 공중에서 안쪽으로 트레이스해서 매달릴 벽을 찾는다
	FHitResult WallHit;
	const FVector WallStart(Probe.X, Probe.Y, FeetZ - 20.f);
	const FVector WallEnd(Location.X, Location.Y, FeetZ - 20.f);
	if (!GetWorld()->LineTraceSingleByChannel(WallHit, WallStart, WallEnd, ECC_WallAttach, Params)) return false;
	if (WallHit.bStartPenetrating) return false;
	if (FMath::Abs(WallHit.ImpactNormal.Z) > 0.1f) return false;
	if (WallHit.ImpactNormal.X * OutDir < 0.9f) return false;
	const float EdgeX = WallHit.ImpactPoint.X;

	// 3. 모서리 윗면 높이
	float LedgeTopZ = FeetZ;
	FHitResult TopHit;
	if (GetWorld()->LineTraceSingleByChannel(TopHit, FVector(EdgeX - OutDir * 5.f, Location.Y, FeetZ + 10.f),
		FVector(EdgeX - OutDir * 5.f, Location.Y, FeetZ - 20.f), PawnChannel, Params, ResponseParams))
	{
		LedgeTopZ = TopHit.ImpactPoint.Z;
	}

	const FVector StepOut(EdgeX + OutDir * (Radius + 1.f), Location.Y, Location.Z + 2.f);
	const FVector Hang(StepOut.X, Location.Y, LedgeTopZ - ResolveHangDepth(HalfHeight));

	// 4. 밖으로 나가는 경로와 내려가는 경로가 막혀 있지 않아야 한다
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(FMath::Max(1.f, Radius - 2.f), FMath::Max(1.f, HalfHeight - 2.f));
	FHitResult PathHit;
	if (GetWorld()->SweepSingleByChannel(PathHit, FVector(Location.X, Location.Y, Location.Z + 2.f), StepOut,
		FQuat::Identity, PawnChannel, Shape, Params, ResponseParams)) return false;
	if (GetWorld()->SweepSingleByChannel(PathHit, StepOut, Hang,
		FQuat::Identity, PawnChannel, Shape, Params, ResponseParams)) return false;

	// 5. 매달린 높이에도 벽이 있어야 한다 (매달리기 판정과 같은 채널)
	FHitResult HangWallHit;
	if (!GetWorld()->LineTraceSingleByChannel(HangWallHit, Hang, Hang - Dir * (Radius + 20.f), ECC_WallAttach, Params)) return false;

	// 6. 매달린 발밑이 바닥과 충분히 떨어져 있어야 한다 (너무 낮은 턱이면 그냥 내려가면 된다)
	const FVector HangFeet = Hang - FVector(0.f, 0.f, HalfHeight);
	FHitResult GroundHit;
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, HangFeet, HangFeet - FVector(0.f, 0.f, MinHeightAboveGround), ECC_Visibility, Params)) return false;

	OutPlan.StepOutLocation = StepOut;
	OutPlan.EdgeStandLocation = Location;
	OutPlan.HangLocation = Hang;
	// 매달리기 Idle은 메시를 높이 오프셋만큼 올리므로 내려가기는 그만큼 위에서 끝낸다
	OutPlan.MeshZOffset = GetHangMeshZOffset(Player);
	OutPlan.DropEndLocation = Hang + FVector(0.f, 0.f, OutPlan.MeshZOffset);
	OutPlan.OutDir = OutDir;
	return true;
}
