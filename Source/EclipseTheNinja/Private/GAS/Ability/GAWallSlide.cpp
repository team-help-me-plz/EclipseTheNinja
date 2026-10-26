


#include "GAS/Ability/GAWallSlide.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Ability/GAWallAttach.h"
#include "GAS/Ability/GALedgeHang.h"
#include "NativeGameplayTags.h"
#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "GAS/StatAttributeSet.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallSlide, "Ability.Movement.WallSlide");

UGAWallSlide::UGAWallSlide()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;


	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_WallSlide);
	SetAssetTags(Tags);

	// 매달린 상태에서만 슬라이등을 시작한다. 시작하면 매달리기는 취소된다 (ActivateAbility 전에 끝난다).
	ActivationRequiredTags.AddTag(UGAWallAttach::GetWallAttachingTag());
	CancelAbilitiesWithTag.AddTag(UGAWallAttach::GetWallAttachAbilityTag());

	// 올라가는 동안 공격·암살과 이동·커서 회전을 막는다
	ActivationOwnedTags.AddTag(UGALedgeHang::GetLedgeHangingTag());
	ActivationOwnedTags.AddTag(UGABaseAttack::GetAttackingTag());
	ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());
}

FGameplayTag UGAWallSlide::GetWallSlideAbilityTag()
{
	return TAG_WallSlide;
}

bool UGAWallSlide::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
		return false;

	const ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Character)) return false;

	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(ASC)) return false;

	if (!Character->GetMesh() || !Character->GetMesh()->GetAnimInstance())
		return false;

	if (!ASC->GetSet<UStatAttributeSet>() || ASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f)
		return false;

	return true;
}

void UGAWallSlide::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const ACharacter* Player = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Player))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	FHitResult Hit;
	const FVector Start = ActorInfo->AvatarActor->GetActorLocation();
	const FVector Dir = FVector::DownVector;
	const FVector End = Start + Dir * SlideMaxHeight;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LedgeClimb), false, Player);
}

void UGAWallSlide::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo * ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{

}
