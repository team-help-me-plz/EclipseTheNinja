#include "GAS/Animation/AnimNotify_ImpactFrame.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

UAnimNotify_ImpactFrame::UAnimNotify_ImpactFrame()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(255, 220, 40);
#endif
}

void UAnimNotify_ImpactFrame::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// 서브시스템은 게임 월드에만 있으므로 에디터 미리보기에서는 재생하지 않는다.
	UWorld* World = MeshComp ? MeshComp->GetWorld() : nullptr;
	UImpactFrameSubsystem* Subsystem = World ? World->GetSubsystem<UImpactFrameSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	Subsystem->PlayImpactFrame(Settings);
}

FString UAnimNotify_ImpactFrame::GetNotifyName_Implementation() const
{
	return TEXT("Impact Frame");
}
