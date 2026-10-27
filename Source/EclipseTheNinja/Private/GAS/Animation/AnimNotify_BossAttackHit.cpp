#include "GAS/Animation/AnimNotify_BossAttackHit.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GameFramework/Character.h"

UAnimNotify_BossAttackHit::UAnimNotify_BossAttackHit()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(220, 60, 60);
#endif
}

void UAnimNotify_BossAttackHit::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::Notify(MeshComp, Animation, EventReference);

	ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	if (!Character || MeshComp != Character->GetMesh())
	{
		return;
	}

	// 받는 어빌리티가 이 노티파이의 판정 영역을 읽을 수 있게 노티파이 자신을 함께 보낸다.
	FGameplayEventData Payload;
	Payload.EventTag = UGABaseAttack::GetHitEventTag();
	Payload.Instigator = Character;
	Payload.OptionalObject = this;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Character, Payload.EventTag, Payload);
}

FString UAnimNotify_BossAttackHit::GetNotifyName_Implementation() const
{
	return TEXT("Boss Attack Hit");
}
