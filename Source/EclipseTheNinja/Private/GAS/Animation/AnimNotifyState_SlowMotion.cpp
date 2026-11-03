#include "GAS/Animation/AnimNotifyState_SlowMotion.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// 애니메이션 에디터 미리보기 월드는 건드리지 않는다.
	UWorld* GetGameWorld(const USkeletalMeshComponent* MeshComp)
	{
		UWorld* World = MeshComp ? MeshComp->GetWorld() : nullptr;
		return World && World->IsGameWorld() ? World : nullptr;
	}
}

void UAnimNotifyState_SlowMotion::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UWorld* World = GetGameWorld(MeshComp))
	{
		UGameplayStatics::SetGlobalTimeDilation(World, FMath::Clamp(TimeDilation, 0.01f, 1.0f));
	}
}

void UAnimNotifyState_SlowMotion::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference
)
{
	// 몽타주가 끊겨도 NotifyEnd가 불리므로 여기서 항상 되돌린다.
	if (UWorld* World = GetGameWorld(MeshComp))
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UAnimNotifyState_SlowMotion::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Slow Motion x%.2f"), TimeDilation);
}
