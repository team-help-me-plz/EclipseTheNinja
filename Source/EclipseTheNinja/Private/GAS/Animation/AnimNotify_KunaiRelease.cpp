#include "GAS/Animation/AnimNotify_KunaiRelease.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GAS/Ability/GAThrowKunai.h"

void UAnimNotify_KunaiRelease::Notify(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
    if (!Character || MeshComp != Character->GetMesh())
    {
        return;
    }

    FGameplayEventData Payload;
    Payload.EventTag = UGAThrowKunai::GetReleaseEventTag();
    Payload.Instigator = Character;
    Payload.OptionalObject = Animation;

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Character, Payload.EventTag, Payload);
}

FString UAnimNotify_KunaiRelease::GetNotifyName_Implementation() const
{
    return TEXT("Kunai Release");
}
