


#include "Character/PlayerCharacterAnimInstance.h"
#include "Character/PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystemComponent.h"
#include "GAS/Ability/GAWallAttach.h"
#include "GAS/Ability/GALedgeHang.h"
#include "GAS/Ability/GAWallSlide.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/BoneReference.h"
#include "BonePose.h"
#include "Engine/World.h"

namespace
{
	/** 그래프가 만든 최종 포즈에 골반 낮추기와 두 다리 IK를 적용하는 프록시 */
	struct FPlayerFootIKProxy : public FAnimInstanceProxy
	{
		explicit FPlayerFootIKProxy(UAnimInstance* InAnimInstance)
			: FAnimInstanceProxy(InAnimInstance)
		{
		}

		// 게임 스레드에서 매 프레임 채운다. 높이는 월드 Z(uu), 법선은 월드 방향이다.
		bool bFootIKActive = false;
		float LeftOffset = 0.0f;
		float RightOffset = 0.0f;
		float PelvisOffset = 0.0f;
		FVector LeftNormal = FVector::UpVector;
		FVector RightNormal = FVector::UpVector;
		float MaxSlopeAngle = 35.0f;
		FName Pelvis, LeftThigh, LeftCalf, LeftFoot, RightThigh, RightCalf, RightFoot;

		virtual bool Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* InRootNode) override
		{
			// 메인 그래프만 처리한다. 링크된 레이어 등 다른 루트는 엔진 기본 경로로 둔다.
			if (!bFootIKActive || InRootNode == nullptr || InRootNode != GetRootNode())
			{
				return false;
			}

			EvaluateAnimationNode_WithRoot(Output, InRootNode);
			ApplyFootIK(Output.Pose);
			return true;
		}

	private:
		struct FLegBones
		{
			FCompactPoseBoneIndex Thigh = FCompactPoseBoneIndex(INDEX_NONE);
			FCompactPoseBoneIndex Calf = FCompactPoseBoneIndex(INDEX_NONE);
			FCompactPoseBoneIndex Foot = FCompactPoseBoneIndex(INDEX_NONE);
		};

		static FCompactPoseBoneIndex FindBone(const FBoneContainer& Bones, const FName Name)
		{
			FBoneReference Ref(Name);
			Ref.Initialize(Bones);
			return Ref.IsValidToEvaluate(Bones) ? Ref.GetCompactPoseIndex(Bones) : FCompactPoseBoneIndex(INDEX_NONE);
		}

		static bool IsValidLeg(const FLegBones& Leg)
		{
			return Leg.Thigh.IsValid() && Leg.Calf.IsValid() && Leg.Foot.IsValid()
				&& Leg.Thigh < Leg.Calf && Leg.Calf < Leg.Foot;
		}

		/** 바닥 법선을 컴포넌트 공간 회전으로 바꾼다. 최대 각도로 자른다 */
		FQuat GetSlopeRotation(const FVector& WorldNormal) const
		{
			const FTransform& ToComponent = GetComponentTransform();
			const FVector Up = ToComponent.InverseTransformVectorNoScale(FVector::UpVector).GetSafeNormal();
			const FVector Normal = ToComponent.InverseTransformVectorNoScale(WorldNormal).GetSafeNormal();
			if (Up.IsNearlyZero() || Normal.IsNearlyZero())
			{
				return FQuat::Identity;
			}

			const FQuat Full = FQuat::FindBetweenNormals(Up, Normal);
			FVector Axis;
			float Angle;
			Full.ToAxisAndAngle(Axis, Angle);
			const float MaxAngle = FMath::DegreesToRadians(MaxSlopeAngle);
			return Angle > MaxAngle ? FQuat(Axis, MaxAngle) : Full;
		}

		/** 무릎이 굽은 평면을 유지한 채 발을 목표 위치로 옮기는 두 뼈 IK */
		static void SolveLeg(FCSPose<FCompactPose>& CSPose, const FLegBones& Leg, const FVector& Effector, const FQuat& FootRotation)
		{
			const FTransform ThighTM = CSPose.GetComponentSpaceTransform(Leg.Thigh);
			const FTransform CalfTM = CSPose.GetComponentSpaceTransform(Leg.Calf);
			const FTransform FootTM = CSPose.GetComponentSpaceTransform(Leg.Foot);

			const FVector Root = ThighTM.GetLocation();
			const FVector Joint = CalfTM.GetLocation();
			const FVector End = FootTM.GetLocation();

			const float UpperLength = FVector::Dist(Root, Joint);
			const float LowerLength = FVector::Dist(Joint, End);
			if (UpperLength < KINDA_SMALL_NUMBER || LowerLength < KINDA_SMALL_NUMBER)
			{
				return;
			}

			const FVector ToEffector = Effector - Root;
			const FVector Direction = ToEffector.GetSafeNormal();
			if (Direction.IsNearlyZero())
			{
				return;
			}

			const float MinReach = FMath::Abs(UpperLength - LowerLength) + 0.1f;
			const float MaxReach = UpperLength + LowerLength - 0.1f;
			const float Reach = FMath::Clamp(ToEffector.Size(), MinReach, MaxReach);

			// 지금 무릎 방향을 굽힘 방향으로 쓴다. 다리가 거의 펴져 있으면 원래 무릎-발 방향으로 대신한다.
			FVector BendDirection = (Joint - Root) - Direction * FVector::DotProduct(Joint - Root, Direction);
			if (!BendDirection.Normalize())
			{
				BendDirection = (Joint - End) - Direction * FVector::DotProduct(Joint - End, Direction);
				if (!BendDirection.Normalize())
				{
					return;
				}
			}

			const float CosUpper = FMath::Clamp(
				(UpperLength * UpperLength + Reach * Reach - LowerLength * LowerLength) / (2.0f * UpperLength * Reach), -1.0f, 1.0f);
			const float SinUpper = FMath::Sqrt(FMath::Max(0.0f, 1.0f - CosUpper * CosUpper));

			const FVector NewJoint = Root + Direction * (UpperLength * CosUpper) + BendDirection * (UpperLength * SinUpper);
			const FVector NewEnd = Root + Direction * Reach;

			FTransform NewThigh = ThighTM;
			NewThigh.SetRotation((FQuat::FindBetweenNormals((Joint - Root).GetSafeNormal(), (NewJoint - Root).GetSafeNormal()) * ThighTM.GetRotation()).GetNormalized());

			FTransform NewCalf = CalfTM;
			NewCalf.SetLocation(NewJoint);
			NewCalf.SetRotation((FQuat::FindBetweenNormals((End - Joint).GetSafeNormal(), (NewEnd - NewJoint).GetSafeNormal()) * CalfTM.GetRotation()).GetNormalized());

			FTransform NewFoot = FootTM;
			NewFoot.SetLocation(NewEnd);
			NewFoot.SetRotation(FootRotation);

			const FBoneTransform Transforms[] = {
				FBoneTransform(Leg.Thigh, NewThigh),
				FBoneTransform(Leg.Calf, NewCalf),
				FBoneTransform(Leg.Foot, NewFoot)
			};
			CSPose.SafeSetCSBoneTransforms(Transforms);
		}

		void ApplyFootIK(FCompactPose& Pose) const
		{
			if (!Pose.IsValid())
			{
				return;
			}

			const FBoneContainer& Bones = Pose.GetBoneContainer();
			const FCompactPoseBoneIndex PelvisIndex = FindBone(Bones, Pelvis);
			const FLegBones Legs[2] = {
				{ FindBone(Bones, LeftThigh), FindBone(Bones, LeftCalf), FindBone(Bones, LeftFoot) },
				{ FindBone(Bones, RightThigh), FindBone(Bones, RightCalf), FindBone(Bones, RightFoot) }
			};
			if (!PelvisIndex.IsValid() || !IsValidLeg(Legs[0]) || !IsValidLeg(Legs[1]))
			{
				return;
			}

			const FTransform& ToComponent = GetComponentTransform();
			const float Offsets[2] = { LeftOffset, RightOffset };
			const FVector Normals[2] = { LeftNormal, RightNormal };

			FCSPose<FCompactPose> CSPose;
			CSPose.InitPose(Pose);

			// 애니메이션이 만든 원래 발 위치에서 바닥까지의 높이 차만큼 발 목표를 옮긴다.
			FVector Effectors[2];
			FQuat FootRotations[2];
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const FTransform FootTM = CSPose.GetComponentSpaceTransform(Legs[Side].Foot);
				Effectors[Side] = FootTM.GetLocation() + ToComponent.InverseTransformVector(FVector(0.0f, 0.0f, Offsets[Side]));
				FootRotations[Side] = (GetSlopeRotation(Normals[Side]) * FootTM.GetRotation()).GetNormalized();
			}

			// 낮은 쪽 발이 닿을 수 있게 골반을 먼저 내린다.
			if (!FMath::IsNearlyZero(PelvisOffset))
			{
				FTransform PelvisTM = CSPose.GetComponentSpaceTransform(PelvisIndex);
				PelvisTM.AddToTranslation(ToComponent.InverseTransformVector(FVector(0.0f, 0.0f, PelvisOffset)));
				const FBoneTransform PelvisTransform[] = { FBoneTransform(PelvisIndex, PelvisTM) };
				CSPose.SafeSetCSBoneTransforms(PelvisTransform);
			}

			for (int32 Side = 0; Side < 2; ++Side)
			{
				SolveLeg(CSPose, Legs[Side], Effectors[Side], FootRotations[Side]);
			}

			FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CSPose), Pose);
		}
	};
}

FAnimInstanceProxy* UPlayerCharacterAnimInstance::CreateAnimInstanceProxy()
{
	return new FPlayerFootIKProxy(this);
}

void UPlayerCharacterAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FPlayerFootIKProxy*>(InProxy);
}

bool UPlayerCharacterAnimInstance::TraceFootGround(const APlayerCharacter& Character, const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	const UWorld* World = Character.GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PlayerFootIK), false, &Character);
	// 지붕 발판 같은 숨긴 보조 박스는 보이는 지붕보다 위에 있을 수 있어 건너뛰고 다시 쏜다.
	for (int32 Attempt = 0; Attempt < 4; ++Attempt)
	{
		if (!World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params))
		{
			return false;
		}

		const UPrimitiveComponent* HitComponent = OutHit.GetComponent();
		const AActor* HitActor = OutHit.GetActor();
		// FootIKGround 태그가 붙은 액터(용마루 위 투명 판 등)는 숨겨져 있어도 발을 디딜 바닥으로 본다.
		static const FName FootIKGroundTag(TEXT("FootIKGround"));
		const bool bFootIKGround = HitActor && HitActor->ActorHasTag(FootIKGroundTag);
		const bool bHidden = !bFootIKGround
			&& ((HitComponent && (HitComponent->bHiddenInGame || !HitComponent->IsVisible()))
				|| (HitActor && HitActor->IsHidden()));
		const bool bIsPawn = HitActor && HitActor->IsA<APawn>();
		if (!bHidden && !bIsPawn)
		{
			return true;
		}

		if (HitComponent)
		{
			Params.AddIgnoredComponent(HitComponent);
		}
		else if (HitActor)
		{
			Params.AddIgnoredActor(HitActor);
		}
		else
		{
			return false;
		}
	}
	return false;
}

void UPlayerCharacterAnimInstance::UpdateFootIK(const APlayerCharacter* Character, float DeltaSeconds)
{
	USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const bool bActive = bEnableFootIK && Character && Mesh && Movement
		&& Movement->IsMovingOnGround() && !bWallAttached;

	float LeftTarget = 0.0f;
	float RightTarget = 0.0f;
	FVector LeftNormalTarget = FVector::UpVector;
	FVector RightNormalTarget = FVector::UpVector;

	if (bActive)
	{
		// 캡슐이 딛고 선 바닥 높이를 애니메이션이 가정하는 바닥으로 본다. 평지에서는 보정이 0이 된다.
		const float CapsuleBottomZ = Character->GetActorLocation().Z - Character->GetSimpleCollisionHalfHeight();
		const float BaseZ = CapsuleBottomZ - (Movement->CurrentFloor.bBlockingHit ? Movement->CurrentFloor.FloorDist : 0.0f);
		auto TraceFoot = [&](const FName FootBone, float& OutOffset, FVector& OutNormal)
		{
			const FVector Foot = Mesh->GetSocketLocation(FootBone);
			FHitResult Hit;
			if (TraceFootGround(*Character, FVector(Foot.X, Foot.Y, BaseZ + FootTraceUp), FVector(Foot.X, Foot.Y, BaseZ - FootTraceDown), Hit))
			{
				OutOffset = FMath::Clamp(Hit.ImpactPoint.Z - BaseZ, -MaxFootDrop, MaxFootRaise);
				OutNormal = Hit.ImpactNormal;
			}
		};
		TraceFoot(LeftFootBoneName, LeftTarget, LeftNormalTarget);
		TraceFoot(RightFootBoneName, RightTarget, RightNormalTarget);
	}

	const float PelvisTarget = FMath::Clamp(FMath::Min3(LeftTarget, RightTarget, 0.0f), -MaxPelvisDrop, 0.0f);

	LeftFootOffset = FMath::FInterpTo(LeftFootOffset, LeftTarget, DeltaSeconds, FootIKInterpSpeed);
	RightFootOffset = FMath::FInterpTo(RightFootOffset, RightTarget, DeltaSeconds, FootIKInterpSpeed);
	PelvisOffset = FMath::FInterpTo(PelvisOffset, PelvisTarget, DeltaSeconds, FootIKInterpSpeed);
	LeftFootNormal = FMath::VInterpNormalRotationTo(LeftFootNormal, LeftNormalTarget.GetSafeNormal(), DeltaSeconds, FootIKInterpSpeed * 20.0f);
	RightFootNormal = FMath::VInterpNormalRotationTo(RightFootNormal, RightNormalTarget.GetSafeNormal(), DeltaSeconds, FootIKInterpSpeed * 20.0f);

	FPlayerFootIKProxy& Proxy = GetProxyOnGameThread<FPlayerFootIKProxy>();
	const bool bHasOffset = !FMath::IsNearlyZero(LeftFootOffset, 0.05f) || !FMath::IsNearlyZero(RightFootOffset, 0.05f)
		|| !FMath::IsNearlyZero(PelvisOffset, 0.05f)
		|| !LeftFootNormal.Equals(FVector::UpVector, 0.001f) || !RightFootNormal.Equals(FVector::UpVector, 0.001f);
	Proxy.bFootIKActive = bEnableFootIK && bHasOffset;
	Proxy.LeftOffset = LeftFootOffset;
	Proxy.RightOffset = RightFootOffset;
	Proxy.PelvisOffset = PelvisOffset;
	Proxy.LeftNormal = LeftFootNormal;
	Proxy.RightNormal = RightFootNormal;
	Proxy.MaxSlopeAngle = MaxFootSlopeAngle;
	Proxy.Pelvis = PelvisBoneName;
	Proxy.LeftThigh = LeftThighBoneName;
	Proxy.LeftCalf = LeftCalfBoneName;
	Proxy.LeftFoot = LeftFootBoneName;
	Proxy.RightThigh = RightThighBoneName;
	Proxy.RightCalf = RightCalfBoneName;
	Proxy.RightFoot = RightFootBoneName;
}

void UPlayerCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	HeadLookPitch = 0.0f;
	HeadLookRotation = FRotator::ZeroRotator;
}

void UPlayerCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{

	// 소유자가 없어지거나 ASC가 없으면 이전 상태를 남기지 않는다.
	const bool bWasCrouching = bIsCrouching;
	bWallAttached = false;
	ClimbingSpeed = 0.f;
	bIsCrouching = false;

	Super::NativeUpdateAnimation(DeltaSeconds);

	// Pawn 재빙의와 에디터 프리뷰에서도 안전하게 갱신한다.
	APlayerCharacter* Character = Cast<APlayerCharacter>(TryGetPawnOwner());

	if (!IsValid(Character))
	{
		HeadLookPitch = 0.0f;
		HeadLookRotation = FRotator::ZeroRotator;
		UpdateFootIK(nullptr, DeltaSeconds);
		return;
	}

	bIsCrouching = Character->bIsCrouched;
	if (bIsCrouching != bWasCrouching)
	{
		SwapTurnClipForCrouch();
	}

	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (IsValid(ASC))
	{
		// 모서리에서 내려가는 중에도 미리 매달리기 상태로 둔다. 내려가기 몽타주가 빠질 때 매달리기 Idle로 바로 섞인다.
		if (ASC->HasMatchingGameplayTag(UGAWallAttach::GetWallAttachingTag())
			|| ASC->HasMatchingGameplayTag(UGALedgeHang::GetLedgeHangPoseTag()) || ASC->HasMatchingGameplayTag(UGAWallSlide::GetWallSlideAbilityTag()))
		{
			bWallAttached = true;
		}

		const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		if (ASC->HasMatchingGameplayTag(UGAWallAttach::GetWallAttachingTag())
			&& Movement->MovementMode == MOVE_Flying
			&& !Movement->CurrentRootMotion.HasActiveRootMotionSources())
		{
			ClimbingSpeed = Movement->Velocity.Z;
		}
	}

	// 캐릭터 고개 상하 지정
	const float TargetPitch = bEnableHeadTracking ? CalculateHeadPitch(*Character) : 0.0f;
	HeadLookPitch = FMath::FInterpTo(HeadLookPitch, TargetPitch, DeltaSeconds, HeadTrackingSpeed);
	HeadLookPitch = FMath::Clamp(HeadLookPitch,
		-FMath::Clamp(MaxHeadPitchDown, 0.0f, 89.0f), FMath::Clamp(MaxHeadPitchUp, 0.0f, 89.0f));

	// 머리 위치 업데이트
	UpdateHeadRotation(*Character);

	// 벽 붙기 여부가 정해진 뒤에 발 IK를 갱신한다.
	UpdateFootIK(Character, DeltaSeconds);

}

void UPlayerCharacterAnimInstance::NativeUninitializeAnimation()
{
	StopTurnInPlace();
	Super::NativeUninitializeAnimation();
}

UAnimSequence* UPlayerCharacterAnimInstance::GetTurnInPlaceAnimation() const
{
	return bIsCrouching && CrouchTurnInPlaceAnimation ? CrouchTurnInPlaceAnimation.Get() : TurnInPlaceAnimation.Get();
}

float UPlayerCharacterAnimInstance::GetTurnInPlacePlayRate() const
{
	const float PlayRate = bIsCrouching && CrouchTurnInPlaceAnimation ? CrouchTurnInPlacePlayRate : TurnInPlacePlayRate;
	return FMath::Max(0.1f, PlayRate);
}

bool UPlayerCharacterAnimInstance::TryStartTurnInPlace(float TargetYaw)
{
	UAnimSequence* TurnAnimation = GetTurnInPlaceAnimation();
	if (!bEnableTurnInPlace || !TurnAnimation || !TurnAnimation->HasRootMotion()
		|| IsAnyMontagePlaying())
	{
		return false;
	}

	// Idle과 턴의 포즈 차이를 관성 블렌드로 부드럽게 연결한다.
	FMontageBlendSettings BlendIn(FMath::Max(0.0f, TurnInPlaceBlendInTime));
	FMontageBlendSettings BlendOut(FMath::Max(0.0f, TurnInPlaceBlendOutTime));
	BlendIn.BlendMode = EMontageBlendMode::Inertialization;
	BlendOut.BlendMode = EMontageBlendMode::Inertialization;
	ActiveTurnMontage = PlaySlotAnimationAsDynamicMontage_WithBlendSettings(
		TurnAnimation, TurnInPlaceSlotName, BlendIn, BlendOut,
		GetTurnInPlacePlayRate(), 1, 0.0f);
	ActiveTurnAnimation = ActiveTurnMontage ? TurnAnimation : nullptr;
	TurnTargetYaw = TargetYaw;
	return ActiveTurnMontage != nullptr;
}

void UPlayerCharacterAnimInstance::CacheTurnYawCurve(UAnimSequence* TurnAnimation)
{
	if (CachedTurnAnimation.Get() == TurnAnimation && TurnYawCurve.Num() > 0)
	{
		return;
	}

	TurnYawCurve.Reset();
	CachedTurnAnimation = TurnAnimation;
	if (!TurnAnimation)
	{
		return;
	}

	// 구간별 회전을 더해 누적 곡선을 만든다. 180도 회전은 한 번에 뽑으면 부호가 모호하기 때문이다.
	constexpr int32 SampleCount = 64;
	const float Length = TurnAnimation->GetPlayLength();
	TurnYawCurveStep = Length / SampleCount;
	const FAnimExtractContext Context;
	TurnYawCurve.Add(0.0f);
	for (int32 Index = 1; Index <= SampleCount; ++Index)
	{
		const FTransform Delta = TurnAnimation->ExtractRootMotionFromRange(
			(Index - 1) * TurnYawCurveStep, Index * TurnYawCurveStep, Context);
		TurnYawCurve.Add(TurnYawCurve.Last() + Delta.GetRotation().Rotator().Yaw);
	}
}

bool UPlayerCharacterAnimInstance::TryStartTurnInPlaceRemaining(float TargetYaw, float RemainingDeltaYaw)
{
	UAnimSequence* TurnAnimation = GetTurnInPlaceAnimation();
	if (!bEnableTurnInPlace || !TurnAnimation || !TurnAnimation->HasRootMotion()
		|| IsAnyMontagePlaying())
	{
		return false;
	}

	return PlayTurnClipRemaining(TurnAnimation, TargetYaw, RemainingDeltaYaw);
}

float UPlayerCharacterAnimInstance::GetTurnYawAtTime(float Time) const
{
	if (TurnYawCurve.Num() < 2 || TurnYawCurveStep <= 0.0f)
	{
		return 0.0f;
	}

	const float SampleIndex = FMath::Clamp(Time / TurnYawCurveStep, 0.0f, TurnYawCurve.Num() - 1.0f);
	const int32 LowIndex = FMath::FloorToInt32(SampleIndex);
	const int32 HighIndex = FMath::Min(LowIndex + 1, TurnYawCurve.Num() - 1);
	return FMath::Lerp(TurnYawCurve[LowIndex], TurnYawCurve[HighIndex], SampleIndex - LowIndex);
}

void UPlayerCharacterAnimInstance::SwapTurnClipForCrouch()
{
	// 턴 도중 앉거나 일어나면 서기/앉기 턴 클립을 바꿔 끼워 돌면서 자세가 바뀌게 한다.
	// 그대로 두면 턴 슬롯이 이전 자세의 포즈를 덮어써서 고개나 상체만 반응한다.
	if (!IsTurningInPlace() || !ActiveTurnAnimation)
	{
		return;
	}

	UAnimSequence* NewAnimation = GetTurnInPlaceAnimation();
	if (!NewAnimation || NewAnimation == ActiveTurnAnimation || !NewAnimation->HasRootMotion())
	{
		return;
	}

	// 지금 클립에서 아직 남은 회전(부호 포함)을 구한다. 역재생 중이면 처음으로 돌아가는 만큼이 남은 회전이다.
	CacheTurnYawCurve(ActiveTurnAnimation);
	if (TurnYawCurve.Num() < 2)
	{
		return;
	}
	const float CurrentYaw = GetTurnYawAtTime(Montage_GetPosition(ActiveTurnMontage));
	const float RemainingDeltaYaw = Montage_GetPlayRate(ActiveTurnMontage) >= 0.0f
		? TurnYawCurve.Last() - CurrentYaw
		: -CurrentYaw;
	if (FMath::Abs(RemainingDeltaYaw) < 1.0f)
	{
		return;
	}

	// 새 클립을 같은 슬롯에 재생하면 이전 턴 몽타주는 관성 블렌드로 빠진다.
	// 남은 회전을 맞출 수 없으면 기존 턴을 그대로 끝까지 재생한다.
	PlayTurnClipRemaining(NewAnimation, TurnTargetYaw, RemainingDeltaYaw);
}

bool UPlayerCharacterAnimInstance::PlayTurnClipRemaining(UAnimSequence* TurnAnimation, float TargetYaw, float RemainingDeltaYaw)
{
	CacheTurnYawCurve(TurnAnimation);
	if (TurnYawCurve.Num() < 2)
	{
		return false;
	}

	const float TotalYaw = TurnYawCurve.Last();
	if (FMath::IsNearlyZero(TotalYaw) || FMath::Abs(RemainingDeltaYaw) > FMath::Abs(TotalYaw))
	{
		return false;
	}

	// 클립과 같은 방향: 이미 돈 만큼 지난 지점부터 정재생하면 남은 회전만 적용된다.
	// 클립과 반대 방향: 누적 회전이 남은 각도와 같은 지점부터 처음까지 역재생하면 반대로 그만큼 돈다.
	const bool bReverse = FMath::Sign(TotalYaw) != FMath::Sign(RemainingDeltaYaw);
	const float StartYaw = bReverse
		? FMath::Abs(RemainingDeltaYaw)
		: FMath::Abs(TotalYaw) - FMath::Abs(RemainingDeltaYaw);
	float StartTime = 0.0f;
	for (int32 Index = 0; Index < TurnYawCurve.Num(); ++Index)
	{
		if (FMath::Abs(TurnYawCurve[Index]) >= StartYaw)
		{
			StartTime = Index * TurnYawCurveStep;
			break;
		}
	}

	FMontageBlendSettings BlendIn(FMath::Max(0.0f, TurnInPlaceBlendInTime));
	FMontageBlendSettings BlendOut(FMath::Max(0.0f, TurnInPlaceBlendOutTime));
	BlendIn.BlendMode = EMontageBlendMode::Inertialization;
	BlendOut.BlendMode = EMontageBlendMode::Inertialization;
	UAnimMontage* NewMontage = PlaySlotAnimationAsDynamicMontage_WithBlendSettings(
		TurnAnimation, TurnInPlaceSlotName, BlendIn, BlendOut,
		GetTurnInPlacePlayRate(), 1, 0.0f, StartTime);
	if (!NewMontage)
	{
		return false;
	}
	ActiveTurnMontage = NewMontage;
	ActiveTurnAnimation = TurnAnimation;
	if (bReverse)
	{
		// 우클릭 턴의 방향 전환과 같은 방식으로 역재생한다.
		Montage_SetPlayRate(ActiveTurnMontage, -GetTurnInPlacePlayRate());
	}
	TurnTargetYaw = TargetYaw;
	return true;
}

void UPlayerCharacterAnimInstance::UpdateTurnInPlaceTarget(float TargetYaw)
{
	if (!IsTurningInPlace()
		|| FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(TurnTargetYaw, TargetYaw)))
	{
		return;
	}

	// 포즈를 유지한 채 역재생하여 발 디딤과 루트 회전을 함께 되돌린다.
	const float CurrentPlayRate = Montage_GetPlayRate(ActiveTurnMontage);
	Montage_SetPlayRate(ActiveTurnMontage, -CurrentPlayRate);
	TurnTargetYaw = TargetYaw;
}

bool UPlayerCharacterAnimInstance::IsTurningInPlace() const
{
	return ActiveTurnMontage && Montage_IsPlaying(ActiveTurnMontage);
}

void UPlayerCharacterAnimInstance::StopTurnInPlace()
{
	// 현재 회전 애니메이션이 재생 중이면
	if (ActiveTurnMontage && Montage_IsActive(ActiveTurnMontage))
	{
		// 우클릭 해제나 이동 입력 후에는 루트 모션 회전도 멈춘다.
		if (FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(ActiveTurnMontage))
		{
			MontageInstance->PushDisableRootMotion();
		}
		FMontageBlendSettings BlendOut(FMath::Max(0.0f, TurnInPlaceBlendOutTime));
		BlendOut.BlendMode = EMontageBlendMode::Inertialization;
		Montage_StopWithBlendSettings(BlendOut, ActiveTurnMontage);
	}
	ActiveTurnMontage = nullptr;
	ActiveTurnAnimation = nullptr;
}

void UPlayerCharacterAnimInstance::CancelTurnInPlace(float BlendOutTime)
{
	if (ActiveTurnMontage && Montage_IsActive(ActiveTurnMontage))
	{
		FMontageBlendSettings BlendOut(FMath::Max(0.0f, BlendOutTime));
		BlendOut.BlendMode = EMontageBlendMode::Inertialization;
		Montage_StopWithBlendSettings(BlendOut, ActiveTurnMontage);
	}
	ActiveTurnMontage = nullptr;
	ActiveTurnAnimation = nullptr;
}

float UPlayerCharacterAnimInstance::CalculateHeadPitch(const APlayerCharacter& Character) const
{
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!Mesh || !Character.HasCursorTarget() || Character.HasWallJumped())
	{
		return 0.0f;
	}

	// head bone의 위치를 가져옴
	const FVector HeadLocation = Mesh->DoesSocketExist(HeadBoneName) ? Mesh->GetSocketLocation(HeadBoneName) : Character.GetPawnViewLocation();

	// head bone에서 커서까지의 방향 벡터
	const FVector ToCursor = Character.GetCursorWorldLocation() - HeadLocation;

	// 위에서 구한 방향 벡터와 전방벡터를 내적
	const float ForwardDistance = FVector::DotProduct(ToCursor, Character.GetActorForwardVector());

	// 내적값이 작으면 같은 선상에 있으니까 0도 반환
	if (ForwardDistance <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	// 내적값을 기반으로 Pitch값 계산
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(ToCursor.Z, ForwardDistance));
	return FMath::Clamp(Pitch,
		-FMath::Clamp(MaxHeadPitchDown, 0.0f, 89.0f), FMath::Clamp(MaxHeadPitchUp, 0.0f, 89.0f));
}

void UPlayerCharacterAnimInstance::UpdateHeadRotation(const APlayerCharacter& Character)
{
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!Mesh)
	{
		HeadLookRotation = FRotator::ZeroRotator;
		return;
	}

	// Mesh의 -90도 회전도 반영하여 고개 회전축을 변환한다.
	const FQuat ActorRotation = Character.GetActorQuat();
	const FQuat WorldDelta = ActorRotation * FRotator(HeadLookPitch, 0.0f, 0.0f).Quaternion() * ActorRotation.Inverse();
	const FQuat MeshRotation = Mesh->GetComponentQuat();
	HeadLookRotation = (MeshRotation.Inverse() * WorldDelta * MeshRotation).Rotator();
}

