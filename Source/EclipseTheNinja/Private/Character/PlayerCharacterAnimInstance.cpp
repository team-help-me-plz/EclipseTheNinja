


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

void UPlayerCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	HeadLookPitch = 0.0f;
	HeadLookRotation = FRotator::ZeroRotator;

	// 애님 인스턴스가 다시 만들어져도 지난 앉은 착지를 새 착지로 착각하지 않게 캐릭터의 횟수에 맞춘다.
	bIsCrouchFalling = false;
	bIsCrouchLanding = false;
	LastCrouchLandCount = 0;
	if (const APlayerCharacter* Character = Cast<APlayerCharacter>(TryGetPawnOwner()))
	{
		LastCrouchLandCount = Character->GetCrouchLandCount();
	}
}

void UPlayerCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{

	// 소유자가 없어지거나 ASC가 없으면 이전 상태를 남기지 않는다.
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
		bIsCrouchFalling = false;
		bIsCrouchLanding = false;
		return;
	}

	bIsCrouching = Character->bIsCrouched;
	UpdateCrouchLanding(*Character, DeltaSeconds);

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

}

void UPlayerCharacterAnimInstance::UpdateCrouchLanding(const APlayerCharacter& Character, float DeltaSeconds)
{
	const UCharacterMovementComponent* Movement = Character.GetCharacterMovement();
	const bool bIsFalling = Movement && Movement->IsFalling();

	// 앉은 채 모서리에서 떨어지는 동안 낙하 반복을 앉은 낙하 클립으로 바꾼다.
	// 공중에서는 캡슐이 일어서 있으므로 bIsCrouching이 아니라 캐릭터의 앉은 낙하 상태를 본다.
	bIsCrouchFalling = bIsFalling && Character.IsCrouchFalling();

	// 캐릭터가 앉은 착지를 알린 프레임에만 시작한다.
	// 공중에서 앉기 키를 떼거나 함정으로 안전 지점에 옮겨지면 알리지 않으므로 기존 착지를 쓴다.
	const int32 CharacterCrouchLandCount = Character.GetCrouchLandCount();
	if (CharacterCrouchLandCount != LastCrouchLandCount)
	{
		LastCrouchLandCount = CharacterCrouchLandCount;
		if (CrouchLandAnimation)
		{
			bIsCrouchLanding = true;
			CrouchLandElapsed = 0.0f;
		}
	}

	if (!bIsCrouchLanding)
	{
		return;
	}

	// 다시 떨어지거나 앉기 키를 떼 일어서면 앉은 착지를 끝낸다.
	// 착지 직후에는 캡슐이 다음 이동 갱신 때 앉으므로 bIsCrouching으로 끊지 않는다.
	if (bIsFalling || !Character.IsCrouchInputHeld() || !CrouchLandAnimation)
	{
		bIsCrouchLanding = false;
		return;
	}

	// 시작 프레임에서 끝 프레임까지 재생 위치를 직접 옮긴다. 끝이 시작보다 앞이면 역재생이 된다.
	const float StartTime = GetCrouchLandFrameTime(CrouchLandStartFrame);
	const float EndTime = GetCrouchLandFrameTime(CrouchLandEndFrame);
	const float Duration = FMath::Abs(EndTime - StartTime) / FMath::Max(0.1f, CrouchLandPlayRate);

	CrouchLandElapsed += DeltaSeconds;
	const float Alpha = Duration > KINDA_SMALL_NUMBER
		? FMath::Clamp(CrouchLandElapsed / Duration, 0.0f, 1.0f)
		: 1.0f;
	CrouchLandExplicitTime = FMath::Lerp(StartTime, EndTime, Alpha);

	if (Alpha >= 1.0f)
	{
		bIsCrouchLanding = false;
	}
}

float UPlayerCharacterAnimInstance::GetCrouchLandFrameTime(int32 Frame) const
{
	if (!CrouchLandAnimation)
	{
		return 0.0f;
	}

	const FFrameRate FrameRate = CrouchLandAnimation->GetSamplingFrameRate();
	const float Time = static_cast<float>(FrameRate.AsSeconds(FFrameTime(Frame)));

	return FMath::Clamp(Time, 0.0f, CrouchLandAnimation->GetPlayLength());
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
	ActiveTurnMontage = PlaySlotAnimationAsDynamicMontage_WithBlendSettings(
		TurnAnimation, TurnInPlaceSlotName, BlendIn, BlendOut,
		GetTurnInPlacePlayRate(), 1, 0.0f, StartTime);
	if (ActiveTurnMontage && bReverse)
	{
		// 우클릭 턴의 방향 전환과 같은 방식으로 역재생한다.
		Montage_SetPlayRate(ActiveTurnMontage, -GetTurnInPlacePlayRate());
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

