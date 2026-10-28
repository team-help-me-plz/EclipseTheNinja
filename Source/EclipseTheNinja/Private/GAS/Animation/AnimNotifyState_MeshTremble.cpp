#include "GAS/Animation/AnimNotifyState_MeshTremble.h"

#include "Components/SkeletalMeshComponent.h"

UAnimNotifyState_MeshTremble::UAnimNotifyState_MeshTremble()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(200, 120, 255);
#endif
}

void UAnimNotifyState_MeshTremble::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// 이미 떨고 있으면(구간이 겹치면) 처음 저장한 원래 위치를 유지한다.
	FTrembleState& State = States.FindOrAdd(MeshComp);
	if (State.Duration <= 0.0f)
	{
		State.BaseLocation = MeshComp->GetRelativeLocation();
	}

	State.Duration = FMath::Max(TotalDuration, KINDA_SMALL_NUMBER);
	State.Elapsed = 0.0f;
	State.SinceChange = 1.0f / Frequency;
}

void UAnimNotifyState_MeshTremble::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	FTrembleState* State = MeshComp ? States.Find(MeshComp) : nullptr;
	if (!State)
	{
		return;
	}

	State->Elapsed += FrameDeltaTime;
	State->SinceChange += FrameDeltaTime;

	// 정해진 주기마다만 새 위치를 골라, 프레임레이트와 상관없이 같은 빠르기로 떨게 한다.
	if (State->SinceChange < 1.0f / Frequency)
	{
		return;
	}

	State->SinceChange = 0.0f;

	const float Strength = bRampUp
		? FMath::Clamp(State->Elapsed / State->Duration, 0.2f, 1.0f)
		: 1.0f;

	const FVector Offset(
		FMath::FRandRange(-Amplitude.X, Amplitude.X),
		FMath::FRandRange(-Amplitude.Y, Amplitude.Y),
		FMath::FRandRange(-Amplitude.Z, Amplitude.Z)
	);

	MeshComp->SetRelativeLocation(State->BaseLocation + Offset * Strength);
}

void UAnimNotifyState_MeshTremble::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	FTrembleState State;
	if (MeshComp && States.RemoveAndCopyValue(MeshComp, State))
	{
		MeshComp->SetRelativeLocation(State.BaseLocation);
	}
}

FString UAnimNotifyState_MeshTremble::GetNotifyName_Implementation() const
{
	return TEXT("Mesh Tremble");
}
