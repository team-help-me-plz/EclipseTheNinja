#include "GAS/Animation/AnimNotify_BossAttackHit.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GameFramework/Character.h"

#if WITH_EDITOR
#include "Animation/AnimSingleNodeInstance.h"
#include "SceneManagement.h"
#endif

UAnimNotify_BossAttackHit::UAnimNotify_BossAttackHit()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(220, 60, 60);

	// 지금 테스트 중인 보스 BP. 다른 보스 BP를 쓰면 노티파이마다 바꿔 준다.
	PreviewCharacterClass = TSoftClassPtr<ACharacter>(FSoftObjectPath(
		TEXT("/Game/Blueprint/Week_3/BossEnemyTest/BP_BossEnemyTest.BP_BossEnemyTest_C")
	));
#endif
}

#if WITH_EDITOR
void UAnimNotify_BossAttackHit::DrawInEditor(
	FPrimitiveDrawInterface* PDI,
	USkeletalMeshComponent* MeshComp,
	const UAnimSequenceBase* Animation,
	const FAnimNotifyEvent& NotifyEvent
) const
{
	if (!PDI || !MeshComp)
	{
		return;
	}

	// 게임에서 메시는 액터 기준 MeshRelative 위치에 놓이므로, 미리보기 메시에서 거꾸로 액터 위치를 구한다.
	// 액터 스케일은 메시와 판정 상자(위치·크기)에 똑같이 곱해지므로, 스케일 1인 미리보기 메시 기준으로는 스케일 없이 그리면 된다.
	FTransform MeshRelative(FRotator(0.0f, -90.0f, 0.0f), FVector(0.0f, 0.0f, -90.0f));
	if (const UClass* PreviewClass = PreviewCharacterClass.LoadSynchronous())
	{
		const ACharacter* PreviewCDO = PreviewClass->GetDefaultObject<ACharacter>();
		if (PreviewCDO && PreviewCDO->GetMesh())
		{
			MeshRelative = PreviewCDO->GetMesh()->GetRelativeTransform();
			MeshRelative.SetScale3D(FVector::OneVector);
		}
	}

	FTransform MeshTransform = MeshComp->GetComponentTransform();
	MeshTransform.SetScale3D(FVector::OneVector);
	const FTransform ActorTransform = MeshRelative.Inverse() * MeshTransform;

	const FVector Center = ActorTransform.TransformPosition(HitArea.Offset);
	const FVector Extent = HitArea.HalfExtent.ComponentMax(FVector::OneVector);
	const FQuat Rotation = ActorTransform.GetRotation();

	// 재생 위치가 노티파이 직후면 굵게 그려 지금 판정이 나가는 상자를 구분한다.
	bool bHighlight = false;
	if (const UAnimSingleNodeInstance* PreviewInstance = MeshComp->GetSingleNodeInstance())
	{
		const float CurrentTime = PreviewInstance->GetCurrentTime();
		const float TriggerTime = NotifyEvent.GetTriggerTime();
		bHighlight = CurrentTime >= TriggerTime && CurrentTime <= TriggerTime + PreviewHighlightTime;
	}

	DrawOrientedWireBox(
		PDI,
		Center,
		Rotation.GetAxisX(),
		Rotation.GetAxisY(),
		Rotation.GetAxisZ(),
		Extent,
		bHighlight ? FLinearColor::Red : FLinearColor(1.0f, 0.4f, 0.4f, 0.4f),
		SDPG_Foreground,
		bHighlight ? 2.0f : 0.5f
	);
}
#endif

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
