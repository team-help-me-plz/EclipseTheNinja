


#include "Character/PlayerCharacterAnimInstance.h"
#include "Character/PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"

void UPlayerCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	HeadLookPitch = 0.0f;
	HeadLookRotation = FRotator::ZeroRotator;
}

void UPlayerCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Pawn 재빙의와 에디터 프리뷰에서도 안전하게 갱신한다.
	APlayerCharacter* Character = Cast<APlayerCharacter>(TryGetPawnOwner());

	if (!IsValid(Character))
	{
		HeadLookPitch = 0.0f;
		HeadLookRotation = FRotator::ZeroRotator;
		return;
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

