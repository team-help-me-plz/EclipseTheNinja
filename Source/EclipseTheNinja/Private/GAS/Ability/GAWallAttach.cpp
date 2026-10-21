


#include "GAS/Ability/GAWallAttach.h"
#include "NativeGameplayTags.h"
#include "GAS/Ability/GABaseAttack.h"
#include "Character/PlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "GAS/StatAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallAttached, "State.Movement.WallAttached");

UGAWallAttach::UGAWallAttach()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	ActivationOwnedTags.AddTag(TAG_WallAttached);
	// 벽에 붙은 상태에서는 공격중 태그를 붙여서 공격이 나가지 않도록
	ActivationOwnedTags.AddTag(UGABaseAttack::GetAttackingTag());
	// 공격 중에는 벽에 붙지 않도록
	ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());
}

FGameplayTag UGAWallAttach::GetWallAttachingTag()
{
	return TAG_WallAttached;
}


bool UGAWallAttach::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	if (!ActorInfo) return false;

	const APlayerCharacter* Player = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Player)) return false;

	// 플레이어가 죽은 경우 실행 불가
	const UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(SourceASC)) return false;
	if (SourceASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f) return false;

	// 땅에서 벽에 매달리기를 허용하지 않는 경우 공중에 있는지까지 확인
	if (!bCanAttachOnGround && Player->GetCharacterMovement()->IsMovingOnGround()) return false;

	return FindWall(Player);
}

void UGAWallAttach::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const APlayerCharacter* Player = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Player)) return;

	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->DisableMovement();

}

void UGAWallAttach::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo * ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	const APlayerCharacter* Player = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Player)) return;

	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Walking);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGAWallAttach::OnCompleted()
{
	if (!IsActive()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGAWallAttach::OnInterrupted()
{
	if (!IsActive()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

bool UGAWallAttach::FindWall(const ACharacter* Player) const
{
	// 액터의 위치에서 액터의 전방으로 WallSearchDist 만큼 트레이스
	// 라인? 캡슐?

	const float PlayerFacing = Player->GetActorForwardVector().X;
	const float SearchDir = PlayerFacing > 0 ? 1 : -1;
	const FVector ForwardVector(SearchDir, 0, 0);

	FHitResult Hit;
	FVector Start = Player->GetActorLocation();
	FVector End = Start + ForwardVector * WallSearchDist;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WallAttach), false, Player);

	if (!GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeCapsule(WallSearchRadius, WallSearchHalfHeight), Params))
	{
		return false;
	}

	// 이미 파 묻힌채 시작한거면 안도이ㅣㅣ
	if (Hit.bStartPenetrating) return false;


	// Pawn과의 충돌은 제외
	const auto* HitComponent = Hit.GetComponent();
	if (!IsValid(HitComponent)) return false;
	if (HitComponent->GetCollisionObjectType() == ECC_Pawn) return false;

	// TODO 수직벽 말고 다른 벽을 허용할지

	// 충돌면이 탐색방향과 마주보고 있는지 확인
	const float FacingDot = FVector::DotProduct(ForwardVector, Hit.ImpactNormal);
	if (FacingDot >= 0)  return false;

	return true;
}
