


#include "GAS/Ability/GAWallAttach.h"
#include "NativeGameplayTags.h"
#include "GAS/Ability/GABaseAttack.h"
#include "Character/PlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "GAS/StatAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallAttach, "Ability.Movement.WallAttach");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallAttached, "State.Movement.WallAttached");
static const ECollisionChannel ECC_WallAttach = ECC_GameTraceChannel2;

UGAWallAttach::UGAWallAttach()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	// 암살이 시작될 때 이 태그로 매달리기를 취소한다
	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_WallAttach);
	SetAssetTags(Tags);

	// 매달린 상태에서 공중·아래 암살은 가능해야 하므로 공격중 태그는 붙이지 않는다.
	// 기본 공격은 GABaseAttack이 WallAttached 태그로 막는다.
	ActivationOwnedTags.AddTag(TAG_WallAttached);
	// 공격 중에는 벽에 붙지 않도록
	ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());
}

FGameplayTag UGAWallAttach::GetWallAttachAbilityTag()
{
	return TAG_WallAttach;
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

	FHitResult Hit;
	return FindWall(Player, Hit);
}

void UGAWallAttach::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APlayerCharacter* Player = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	FHitResult WallHit;
	if (!IsValid(Player) || !FindWall(Player, WallHit))
	{
		OnInterrupted();
		return;
	}

	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->DisableMovement();

	// 캡슐은 벽 표면에 딱 붙인다 (벽 속에 넣으면 Falling 복귀 시 튕겨 나옴)
	const float WallNormalX = WallHit.ImpactNormal.X > 0.f ? 1.f : -1.f;
	const float Radius = Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
	FVector AttachPoint = Player->GetActorLocation();
	AttachPoint.X = WallHit.ImpactPoint.X + WallNormalX * Radius;
	Player->SetActorLocation(AttachPoint);

	// 벽을 정면으로 바라보게 해서 메시 오프셋의 전방이 항상 벽 쪽이 되도록
	Player->SetActorRotation(FRotator(0.f, WallNormalX > 0.f ? 180.f : 0.f, 0.f));

	// 애니메이션 위치 보정은 캡슐이 아니라 메시에서 한다
	if (USkeletalMeshComponent* Mesh = Player->GetMesh())
	{
		BaseMeshRelativeLocation = Mesh->GetRelativeLocation();
		Mesh->SetRelativeLocation(BaseMeshRelativeLocation + FVector(AnimXOffset, 0.f, AnimZOffset));
		bMeshOffsetApplied = true;
	}
}

void UGAWallAttach::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo * ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	const APlayerCharacter* Player = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (IsValid(Player))
	{
		UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
		Movement->SetMovementMode(MOVE_Falling);

		if (bMeshOffsetApplied && Player->GetMesh())
		{
			Player->GetMesh()->SetRelativeLocation(BaseMeshRelativeLocation);
		}
	}
	bMeshOffsetApplied = false;
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

bool UGAWallAttach::FindWall(const ACharacter* Player, FHitResult& OutHit) const
{
	// 액터의 위치에서 액터의 전방으로 WallSearchDist 만큼 트레이스

	const float PlayerFacing = Player->GetActorForwardVector().X;
	const float SearchDir = PlayerFacing > 0 ? 1 : -1;
	const FVector ForwardVector(SearchDir, 0, 0);

	FHitResult BodyHit, HeadHit;
	FVector BodyStart = Player->GetActorLocation();
	FVector HeadStart = BodyStart;
	HeadStart.Z += Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.5f;
	FVector BodyEnd = BodyStart + ForwardVector * WallSearchDist;
	FVector HeadEnd = HeadStart + ForwardVector * WallSearchDist;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WallAttach), false, Player);

	if (!GetWorld()->LineTraceSingleByChannel(BodyHit, BodyStart, BodyEnd, ECC_WallAttach, Params))
	{
		return false;
	}

	if (!GetWorld()->LineTraceSingleByChannel(HeadHit, HeadStart, HeadEnd, ECC_WallAttach, Params))
	{
		return false;
	}

	if (FMath::Abs(BodyHit.ImpactNormal.Z) > MaxWallNormalZ) return false;
	if (FMath::Abs(HeadHit.ImpactNormal.Z) > MaxWallNormalZ) return false;

	// 이미 파 묻힌채 시작한거면 안도이ㅣㅣ
	if (BodyHit.bStartPenetrating) return false;
	if (HeadHit.bStartPenetrating) return false;


	// Pawn과의 충돌은 제외
	const auto* BodyHitComponent = BodyHit.GetComponent();
	if (!IsValid(BodyHitComponent)) return false;
	if (BodyHitComponent->GetCollisionObjectType() == ECC_Pawn) return false;

	const auto* HeadHitComponent = HeadHit.GetComponent();
	if (!IsValid(HeadHitComponent)) return false;
	if (HeadHitComponent->GetCollisionObjectType() == ECC_Pawn) return false;

	// 머리와 몸통의 결과가 다른 곳인지 확인 (계단 형태인지 확인)
	if (FMath::Abs(BodyHit.Distance - HeadHit.Distance) > MaxWallDepthDiff) return false;

	// 충돌면이 탐색방향과 마주보고 있는지 확인
	const float FacingDot = FVector::DotProduct(ForwardVector, BodyHit.ImpactNormal);
	if (FacingDot >= 0)  return false;

	// 바닥이 너무 가까우면 못 매달림
	const FVector Feet = BodyStart - FVector(0.f, 0.f, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FHitResult GroundHit;
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, Feet, Feet - FVector(0.f, 0.f, MinHeightAboveGround), ECC_Visibility, Params)) return false;

	OutHit = BodyHit;
	return true;
}
