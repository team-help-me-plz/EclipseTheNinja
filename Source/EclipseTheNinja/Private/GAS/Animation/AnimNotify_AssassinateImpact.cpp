#include "GAS/Animation/AnimNotify_AssassinateImpact.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Ability/GAAssassinateBase.h"
#include "GameFramework/Character.h"

void UAnimNotify_AssassinateImpact::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	if (!Character || MeshComp != Character->GetMesh())
	{
		return;
	}
	FGameplayEventData Payload;
	Payload.EventTag = UGAAssassinateBase::GetImpactEventTag();
	Payload.Instigator = Character;
	Payload.OptionalObject = Animation;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Character, Payload.EventTag, Payload);
}

FString UAnimNotify_AssassinateImpact::GetNotifyName_Implementation() const
{
	return TEXT("Assassinate Impact");
}
