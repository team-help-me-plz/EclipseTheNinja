#include "GAS/Task/AbilityTask_MontageRootOffset.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

UAbilityTask_MontageRootOffset::UAbilityTask_MontageRootOffset()
{
	bTickingTask = true;
}

UAbilityTask_MontageRootOffset* UAbilityTask_MontageRootOffset::FollowMontageRoot(UGameplayAbility* OwningAbility,
	ACharacter* InCharacter, UAnimMontage* InMontage, bool bInSideOnly, bool bInKeepOffsetOnEnd)
{
	UAbilityTask_MontageRootOffset* Task = NewAbilityTask<UAbilityTask_MontageRootOffset>(OwningAbility);
	Task->Character = InCharacter;
	Task->Montage = InMontage;
	Task->bSideOnly = bInSideOnly;
	Task->bKeepOffsetOnEnd = bInKeepOffsetOnEnd;
	return Task;
}

void UAbilityTask_MontageRootOffset::Activate()
{
	Super::Activate();

	ACharacter* Owner = Character.Get();
	if (!IsValid(Owner) || !IsValid(Owner->GetMesh()) || !IsValid(Montage))
	{
		EndTask();
		return;
	}

	BaseRelativeLocation = Owner->GetMesh()->GetRelativeLocation();
	ReturnDuration = FMath::Max(0.01f, Montage->BlendOut.GetBlendTime());
}

void UAbilityTask_MontageRootOffset::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	ACharacter* Owner = Character.Get();
	if (!IsValid(Owner) || !IsValid(Owner->GetMesh()))
	{
		EndTask();
		return;
	}

	const UAnimInstance* AnimInstance = Owner->GetMesh()->GetAnimInstance();
	const FAnimMontageInstance* MontageInstance = AnimInstance ? AnimInstance->GetActiveInstanceForMontage(Montage) : nullptr;

	// 몽타주가 재생 중이면 루트 이동을 따라간다.
	if (MontageInstance && !MontageInstance->IsStopped())
	{
		bReturning = false;
		ApplyOffset(CalculateOffset());
		return;
	}

	// 몽타주가 끝났거나 섞여 빠지는 중이다. 피격자는 그 자리에 두고, 암살자는 블렌드 아웃 동안 원래 자리로 돌아온다.
	if (bKeepOffsetOnEnd) return;

	if (!bReturning)
	{
		bReturning = true;
		ReturnElapsed = 0.0f;
		ReturnStartOffset = CurrentOffset;
	}
	ReturnElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(ReturnElapsed / ReturnDuration, 0.0f, 1.0f);
	ApplyOffset(FMath::Lerp(ReturnStartOffset, FVector::ZeroVector, FMath::SmoothStep(0.0f, 1.0f, Alpha)));
	if (Alpha >= 1.0f)
	{
		EndTask();
	}
}

FVector UAbilityTask_MontageRootOffset::CalculateOffset() const
{
	const ACharacter* Owner = Character.Get();
	const UAnimInstance* AnimInstance = Owner->GetMesh()->GetAnimInstance();
	const float Position = AnimInstance->Montage_GetPosition(Montage);

	// 몽타주 시작부터 현재 위치까지 루트가 움직인 양(애니메이션 공간)을 월드 방향으로 바꾼다.
	const FAnimExtractContext Context;
	const FVector AnimDelta = Montage->ExtractRootMotionFromTrackRange(0.0f, Position, Context).GetTranslation();
	FVector WorldDelta = Owner->GetMesh()->GetComponentQuat().RotateVector(AnimDelta);
	WorldDelta.Z = 0.0f;

	if (bSideOnly)
	{
		const FVector Right = Owner->GetActorRightVector();
		WorldDelta = Right * FVector::DotProduct(WorldDelta, Right);
	}

	// 메시 상대 위치는 액터 공간이다.
	return Owner->GetActorQuat().UnrotateVector(WorldDelta);
}

void UAbilityTask_MontageRootOffset::ApplyOffset(const FVector& Offset)
{
	ACharacter* Owner = Character.Get();
	if (!IsValid(Owner) || !IsValid(Owner->GetMesh())) return;

	CurrentOffset = Offset;
	Owner->GetMesh()->SetRelativeLocation(BaseRelativeLocation + Offset);
}

void UAbilityTask_MontageRootOffset::OnDestroy(bool bInOwnerFinished)
{
	// 피격자가 아니면 메시를 원래 자리로 되돌린다.
	if (!bKeepOffsetOnEnd)
	{
		ApplyOffset(FVector::ZeroVector);
	}

	Super::OnDestroy(bInOwnerFinished);
}
