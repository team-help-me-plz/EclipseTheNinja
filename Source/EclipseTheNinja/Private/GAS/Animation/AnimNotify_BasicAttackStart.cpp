


#include "GAS/Animation/AnimNotify_BasicAttackStart.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GameFramework/Character.h"

void UAnimNotify_BasicAttackStart::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	if (!Character || MeshComp != Character->GetMesh())
	{
		return;
	}
	FGameplayEventData Payload;
	Payload.EventTag = UGABaseAttack::GetAttackStartEventTag();
	Payload.Instigator = Character;
	Payload.OptionalObject = Animation;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Character, Payload.EventTag, Payload);
}
