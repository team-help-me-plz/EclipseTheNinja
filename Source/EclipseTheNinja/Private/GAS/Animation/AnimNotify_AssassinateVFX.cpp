#include "GAS/Animation/AnimNotify_AssassinateVFX.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Ability/GAAssassinateBase.h"
#include "GameFramework/Character.h"

void UAnimNotify_AssassinateVFX::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!IsValid(MeshComp)) return;

	ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner());
	if (!IsValid(Character)) return;
	if (MeshComp != Character->GetMesh()) return;

	// 히트스톱 이벤트와 분리해 VFX만 재생한다.
	FGameplayEventData Payload;
	Payload.EventTag = UGAAssassinateBase::GetVFXEventTag();
	Payload.Instigator = Character;
	Payload.OptionalObject = Animation;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Character, Payload.EventTag, Payload);
}

FString UAnimNotify_AssassinateVFX::GetNotifyName_Implementation() const
{
	return TEXT("Assassinate VFX");
}
