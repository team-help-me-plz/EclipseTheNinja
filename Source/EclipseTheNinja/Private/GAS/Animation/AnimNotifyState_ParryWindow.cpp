#include "GAS/Animation/AnimNotifyState_ParryWindow.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Ability/GAParry.h"

namespace
{
	// 애니메이션 에디터 미리보기처럼 ASC가 없거나 패링 중이 아니면 nullptr을 돌려준다.
	UGAParry* FindParry(const USkeletalMeshComponent* MeshComp)
	{
		AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
		return UGAParry::FindActiveParry(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner));
	}
}

void UAnimNotifyState_ParryWindow::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UGAParry* Parry = FindParry(MeshComp))
	{
		Parry->OpenWindowFromNotify();
	}
}

void UAnimNotifyState_ParryWindow::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference
)
{
	if (UGAParry* Parry = FindParry(MeshComp))
	{
		Parry->CloseWindowFromNotify();
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UAnimNotifyState_ParryWindow::GetNotifyName_Implementation() const
{
	return TEXT("Parry Window");
}
