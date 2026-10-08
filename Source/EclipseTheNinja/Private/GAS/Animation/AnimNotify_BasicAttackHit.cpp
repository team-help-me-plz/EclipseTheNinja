#include "GAS/Animation/AnimNotify_BasicAttackHit.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GameFramework/Character.h"

void UAnimNotify_BasicAttackHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	if (!Character || MeshComp != Character->GetMesh())
	{
		return;
	}
	FGameplayEventData Payload;
	Payload.EventTag = UGABaseAttack::GetHitEventTag();
	Payload.Instigator = Character;
	Payload.OptionalObject = Animation;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Character, Payload.EventTag, Payload);
}

FString UAnimNotify_BasicAttackHit::GetNotifyName_Implementation() const
{
	return TEXT("Basic Attack Hit");
}
