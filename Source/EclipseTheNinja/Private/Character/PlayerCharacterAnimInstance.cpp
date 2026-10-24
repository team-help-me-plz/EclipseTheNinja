


#include "Character/PlayerCharacterAnimInstance.h"
#include "Character/PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystemComponent.h"
#include "GAS/Ability/GAWallAttach.h"
#include "GAS/Ability/GALedgeHang.h"

void UPlayerCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	HeadLookPitch = 0.0f;
	HeadLookRotation = FRotator::ZeroRotator;
}

void UPlayerCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{

	// 소유자가 없어지거나 ASC가 없으면 이전 상태를 남기지 않는다.
	bWallAttached = false;

	Super::NativeUpdateAnimation(DeltaSeconds);

	// Pawn 재빙의와 에디터 프리뷰에서도 안전하게 갱신한다.
	APlayerCharacter* Character = Cast<APlayerCharacter>(TryGetPawnOwner());

	if (!IsValid(Character))
	{
		HeadLookPitch = 0.0f;
		HeadLookRotation = FRotator::ZeroRotator;
		return;
	}

	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (IsValid(ASC))
	{
		// 모서리에서 내려가는 중에도 미리 매달리기 상태로 둔다. 내려가기 몽타주가 빠질 때 매달리기 Idle로 바로 섞인다.
		if (ASC->HasMatchingGameplayTag(UGAWallAttach::GetWallAttachingTag())
			|| ASC->HasMatchingGameplayTag(UGALedgeHang::GetLedgeHangPoseTag()))
		{
			bWallAttached = true;
		}
	}

	// 캐릭터 고개 상하 지정
	const float TargetPitch = bEnableHeadTracking ? CalculateHeadPitch(*Character) : 0.0f;
	HeadLookPitch = FMath::FInterpTo(HeadLookPitch, TargetPitch, DeltaSeconds, HeadTrackingSpeed);
	HeadLookPitch = FMath::Clamp(HeadLookPitch,
		-FMath::Clamp(MaxHeadPitchDown, 0.0f, 89.0f), FMath::Clamp(MaxHeadPitchUp, 0.0f, 89.0f));

	// 머리 위치 업데이트
	UpdateHeadRotation(*Character);

}

void UPlayerCharacterAnimInstance::NativeUninitializeAnimation()
{
	StopTurnInPlace();
	Super::NativeUninitializeAnimation();
}

bool UPlayerCharacterAnimInstance::TryStartTurnInPlace(float TargetYaw)
{
	if (!bEnableTurnInPlace || !TurnInPlaceAnimation || !TurnInPlaceAnimation->HasRootMotion()
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
		TurnInPlaceAnimation, TurnInPlaceSlotName, BlendIn, BlendOut,
		FMath::Max(0.1f, TurnInPlacePlayRate), 1, 0.0f);
	TurnTargetYaw = TargetYaw;
	return ActiveTurnMontage != nullptr;
}

void UPlayerCharacterAnimInstance::CacheTurnYawCurve()
{
	if (CachedTurnAnimation.Get() == TurnInPlaceAnimation && TurnYawCurve.Num() > 0)
	{
		return;
	}

	TurnYawCurve.Reset();
	CachedTurnAnimation = TurnInPlaceAnimation;
	if (!TurnInPlaceAnimation)
	{
		return;
	}

	// 구간별 회전을 더해 누적 곡선을 만든다. 180도 회전은 한 번에 뽑으면 부호가 모호하기 때문이다.
	constexpr int32 SampleCount = 64;
	const float Length = TurnInPlaceAnimation->GetPlayLength();
	TurnYawCurveStep = Length / SampleCount;
	const FAnimExtractContext Context;
	TurnYawCurve.Add(0.0f);
	for (int32 Index = 1; Index <= SampleCount; ++Index)
	{
		const FTransform Delta = TurnInPlaceAnimation->ExtractRootMotionFromRange(
			(Index - 1) * TurnYawCurveStep, Index * TurnYawCurveStep, Context);
		TurnYawCurve.Add(TurnYawCurve.Last() + Delta.GetRotation().Rotator().Yaw);
	}
}

bool UPlayerCharacterAnimInstance::TryStartTurnInPlaceRemaining(float TargetYaw, float RemainingDeltaYaw)
{
	if (!bEnableTurnInPlace || !TurnInPlaceAnimation || !TurnInPlaceAnimation->HasRootMotion()
		|| IsAnyMontagePlaying())
	{
		return false;
	}

	CacheTurnYawCurve();
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
	ActiveTurnMontage = PlaySlotAnimationAsDynamicMontage_WithBlendSettings(
		TurnInPlaceAnimation, TurnInPlaceSlotName, BlendIn, BlendOut,
		FMath::Max(0.1f, TurnInPlacePlayRate), 1, 0.0f, StartTime);
	if (ActiveTurnMontage && bReverse)
	{
		// 우클릭 턴의 방향 전환과 같은 방식으로 역재생한다.
		Montage_SetPlayRate(ActiveTurnMontage, -FMath::Max(0.1f, TurnInPlacePlayRate));
	}
	TurnTargetYaw = TargetYaw;
	return ActiveTurnMontage != nullptr;
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

