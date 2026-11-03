#include "GAS/Animation/AnimNotify_SummonAllies.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GAS/Ability/GASummonAllies.h"

void UAnimNotify_SummonAllies::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// 캐릭터 본체 메시에서 재생될 때만 보낸다. 몽타주 미리보기에서는 무시된다.
	ACharacter* Character = MeshComp
		? Cast<ACharacter>(MeshComp->GetOwner())
		: nullptr;

	if (!Character || MeshComp != Character->GetMesh())
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = UGASummonAllies::GetSummonEventTag();
	Payload.Instigator = Character;
	Payload.OptionalObject = Animation;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Character, Payload.EventTag, Payload);
}

FString UAnimNotify_SummonAllies::GetNotifyName_Implementation() const
{
	return TEXT("Summon Allies");
}
