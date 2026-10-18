


#include "GAS/Ability/GAAssassinate.h"
#include "ProjComponent/AbilityTask_WalkToLocation.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BrainComponent.h"
#include "Character/Enemy.h"
#include "Character/PlayerCharacter.h"
#include "Component/AssassinateTargetComponent.h"
#include "Component/AssassinationTargetingComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/StatAttributeSet.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Assassinating, "State.Assassinating");

UGAAssassinate::UGAAssassinate()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	ActivationOwnedTags.AddTag(TAG_Assassinating);
	ActivationOwnedTags.AddTag(UGABaseAttack::GetAttackingTag());
	ActivationBlockedTags.AddTag(TAG_Assassinating);
	ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());
	AttackMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(
		TEXT("/Game/Anims/Montage/AM_Assassination_Attack.AM_Assassination_Attack")));
	VictimMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(
		TEXT("/Game/Anims/Montage/AM_Assassination_Victim.AM_Assassination_Victim")));
}

FGameplayTag UGAAssassinate::GetAssassinatingTag()
{
	return TAG_Assassinating;
}

bool UGAAssassinate::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	if (!ActorInfo) return false;

	const APlayerCharacter* Player = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Player)) return false;
	if (!Player->GetCharacterMovement()->IsMovingOnGround()) return false;

	const UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(SourceASC)) return false;
	if (SourceASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f) return false;

	UAssassinationTargetingComponent* Targeting = Player->GetAssassinationTargetingComponent();
	if (!IsValid(Targeting)) return false;

	UAssassinateTargetComponent* Target = Targeting->GetCurrentTarget();
	if (!IsValid(Target)) return false;
	if (!Target->CanAssassinate()) return false;

	const AEnemy* Enemy = Cast<AEnemy>(Target->GetOwner());
	if (!IsValid(Enemy)) return false;
	if (!Enemy->GetCharacterMovement()->IsMovingOnGround()) return false;
	if (FMath::Abs(Player->GetActorLocation().Z - Enemy->GetActorLocation().Z) > 45.0f) return false;

	return true;
}

void UGAAssassinate::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	bEnding = false;
	bMovementLocked = false;
	bTargetReserved = false;
	PausedBrain.Reset();
	Assassin = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());

	APlayerCharacter* Player = Assassin.Get();
	if (!IsValid(Player)) { OnInterrupted(); return; }

	UAssassinationTargetingComponent* Targeting = Player->GetAssassinationTargetingComponent();
	if (!IsValid(Targeting)) { OnInterrupted(); return; }

	TargetComponent = Targeting->GetCurrentTarget();
	if (!TargetComponent.IsValid()) { OnInterrupted(); return; }
	if (!TargetComponent->CanAssassinate()) { OnInterrupted(); return; }

	Victim = Cast<AEnemy>(TargetComponent->GetOwner());
	AEnemy* Enemy = Victim.Get();
	if (!IsValid(Enemy)) { OnInterrupted(); return; }

	ExecutionMontage = AttackMontage.LoadSynchronous();
	VictimExecutionMontage = VictimMontage.LoadSynchronous();
	if (!IsValid(ExecutionMontage)) { OnInterrupted(); return; }
	if (!IsValid(VictimExecutionMontage)) { OnInterrupted(); return; }
	if (!Player->GetMesh()->GetAnimInstance()) { OnInterrupted(); return; }
	if (!Enemy->GetMesh()->GetAnimInstance()) { OnInterrupted(); return; }
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) { OnInterrupted(); return; }

	// 실행 중에는 타겟팅 결과가 바뀌어도 처음 확정한 적을 유지한다.
	TargetComponent->SetAssassinationInProgress(true);
	bTargetReserved = true;
	TargetComponent->SetTargetHighlighted(false);
	Enemy->OnDestroyed.AddDynamic(this, &UGAAssassinate::OnVictimDestroyed);
	Enemy->CancelAIBaseAttack();
	if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
	{
		AI->StopMovement();
		UBrainComponent* Brain = AI->GetBrainComponent();
		if (IsValid(Brain) && !Brain->IsPaused())
		{
			Brain->PauseLogic(TEXT("Assassination"));
			PausedBrain = Brain;
		}
	}

	UCharacterMovementComponent* PlayerMovement = Player->GetCharacterMovement();
	PlayerMovementMode = PlayerMovement->MovementMode;
	PlayerCustomMode = PlayerMovement->CustomMovementMode;
	PlayerMovement->StopMovementImmediately();
	Player->StopJumping();

	UCharacterMovementComponent* EnemyMovement = Enemy->GetCharacterMovement();
	VictimMovementMode = EnemyMovement->MovementMode;
	VictimCustomMode = EnemyMovement->CustomMovementMode;
	EnemyMovement->StopMovementImmediately();
	EnemyMovement->DisableMovement();
	bMovementLocked = true;

	ExecutionRotation = Enemy->GetActorRotation();
	Player->SetAssassinationFacing(ExecutionRotation.Yaw);
	const float CapsuleSeparation = Player->GetCapsuleComponent()->GetScaledCapsuleRadius()
		+ Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius() + 5.0f;
	const float DistanceBehind = FMath::Max(BehindDistance, CapsuleSeparation);
	ApproachLocation = Enemy->GetActorLocation() - Enemy->GetActorForwardVector() * DistanceBehind;
	// 사이드뷰의 이동 평면과 현재 바닥 높이를 유지한다.
	ApproachLocation.Y = Player->GetActorLocation().Y;
	ApproachLocation.Z = Player->GetActorLocation().Z;

	if (FVector::DistSquared2D(Player->GetActorLocation(), ApproachLocation) <= FMath::Square(5.0f))
	{
		OnApproachCompleted();
		return;
	}

	UAbilityTask_WalkToLocation* MoveTask = UAbilityTask_WalkToLocation::WalkToLocation(
		this, ApproachLocation, ApproachTimeout);
	MoveTask->OnReached.AddDynamic(this, &UGAAssassinate::OnApproachCompleted);
	MoveTask->OnFailed.AddDynamic(this, &UGAAssassinate::OnInterrupted);
	MoveTask->ReadyForActivation();
}

void UGAAssassinate::OnApproachCompleted()
{
	if (bEnding) return;
	if (!IsActive()) return;

	APlayerCharacter* Player = Assassin.Get();
	if (!IsValid(Player)) { OnInterrupted(); return; }

	AEnemy* Enemy = Victim.Get();
	if (!IsValid(Enemy)) { OnInterrupted(); return; }
	if (Enemy->IsDead()) { OnInterrupted(); return; }

	// 이동 태스크의 도착 판정보다 엄격히 확인해 벽에 막힌 채 처치하지 않는다.
	if (FVector::DistSquared2D(Player->GetActorLocation(), ApproachLocation) > FMath::Square(10.0f))
	{
		OnInterrupted();
		return;
	}

	Player->GetCharacterMovement()->StopMovementImmediately();
	Player->GetCharacterMovement()->DisableMovement();
	Player->SetAssassinationFacing(ExecutionRotation.Yaw);

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, ExecutionMontage, 1.0f, NAME_None, true);
	MontageTask->OnCompleted.AddDynamic(this, &UGAAssassinate::OnCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGAAssassinate::OnInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UGAAssassinate::OnInterrupted);
	MontageTask->ReadyForActivation();
	if (!IsActive()) return;

	// 뒤에 도착해 연출이 시작되는 시점에 기존 사망 처리로 처치를 확정한다.
	Enemy->ReceiveAssassination(VictimExecutionMontage);
}

void UGAAssassinate::OnCompleted()
{
	if (bEnding) return;
	if (!IsActive()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGAAssassinate::OnInterrupted()
{
	if (bEnding) return;
	if (!IsActive()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

void UGAAssassinate::OnVictimDestroyed(AActor* DestroyedActor)
{
	OnInterrupted();
}

void UGAAssassinate::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bEnding) return;
	bEnding = true;

	APlayerCharacter* Player = Assassin.Get();
	if (IsValid(Player) && bMovementLocked)
	{
		UAbilitySystemComponent* SourceASC = Player->GetAbilitySystemComponent();
		if (IsValid(SourceASC) && SourceASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) > 0.0f)
		{
			Player->GetCharacterMovement()->SetMovementMode(PlayerMovementMode, PlayerCustomMode);
		}
	}

	AEnemy* Enemy = Victim.Get();
	if (IsValid(Enemy))
	{
		Enemy->OnDestroyed.RemoveDynamic(this, &UGAAssassinate::OnVictimDestroyed);
		if (!Enemy->IsDead() && bMovementLocked)
		{
			Enemy->GetCharacterMovement()->SetMovementMode(VictimMovementMode, VictimCustomMode);
			if (PausedBrain.IsValid())
			{
				PausedBrain->ResumeLogic(TEXT("Assassination cancelled"));
			}
		}
	}

	if (bTargetReserved && TargetComponent.IsValid())
	{
		TargetComponent->SetAssassinationInProgress(false);
	}

	bMovementLocked = false;
	bTargetReserved = false;
	Assassin.Reset();
	Victim.Reset();
	TargetComponent.Reset();
	PausedBrain.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
