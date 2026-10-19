#include "Ability/GACloneTech.h"
#include "Clone/CloneCharacter.h"
#include "ProjComponent/AbilityTask_WaitCloneTargeting.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "Character/PlayerCharacter.h"
#include "Interface/Interactable.h"
#include "ObjectPool/ObjectPoolSubsystem.h"
#include "Character/PlayerCharacter.h"

#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Components/CapsuleComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "AbilitySystemComponent.h"

UGACloneTech::UGACloneTech()
{
	this->InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGACloneTech::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return ;
	}
	if (!Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[UGACloneTech::ActivateAbility] Ability를 불러온 Actor가 유효하지 않습니다")
		);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return ;
	}
	if (!this->CloneClass)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[UGACloneTech::ActivateAbility] CloneClass가 nullptr입니다")
		);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return ;
	}
	if (!this->TargetingMappingContext || !this->AcceptInputAction || !this->CancelInputAction)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[UGACloneTech::ActivateAbility] Input 설정이 완료되지 않았습니다")
		);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return ;
	}

	this->bIsSwaped = false;
	this->SpawnedClone.Reset();
	this->TargetingTask = UAbilityTask_WaitCloneTargeting::WaitCloneTarget(
		this, this->TargetingMappingContext.Get(), this->AcceptInputAction.Get(),
		this->CancelInputAction.Get(), this->DestinationSystem.Get(), 
		this->TargetingMappingPriority, this->TargetingTimeDilation);
	if (!this->TargetingTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return ;
	}

	this->TargetingTask->OnAccepted.AddDynamic(this, &UGACloneTech::HandleTargetAccepted);
	this->TargetingTask->OnCancelled.AddDynamic(this, &UGACloneTech::HandleTargetCancelled);
	SetAvatarCloneTargeting(true);
	this->TargetingTask->ReadyForActivation();
}

void UGACloneTech::HandleTargetAccepted(const FCloneCommandType& InCommand)
{
	if (!IsActive())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[UGACloneTech::HandleTargetAccepted] Ability가 Active 상태가 아닙니다")
		);
		return ;
	}
	if (!this->TargetingTask)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[UGACloneTech::HandleTargetAccepted] TargetingTask가 nullptr입니다")
		);
		return ;
	}
	this->TargetingTask = nullptr;
	SetAvatarCloneTargeting(false);

	ACharacter*	Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());

	switch (InCommand.Type)
	{
	case(ECloneCommandType::Move):
	{
		if (InCommand.TargetLocation.ContainsNaN())
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[UGACloneTech::HandleTargetAccepted] 잘못된 위치가 입력됐습니다")
			);
			EndAbility(this->CurrentSpecHandle, this->CurrentActorInfo, this->CurrentActivationInfo, true, true);
			return;
		}
		break ;
	}
	case(ECloneCommandType::Interact):
	{
		if (!IsValid(InCommand.TargetActor))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[UGACloneTech::HandleTargetAccepted] TargetActor가 nullptr입니다")
			);
			EndAbility(this->CurrentSpecHandle, this->CurrentActorInfo, this->CurrentActivationInfo, true, true);
			return;
		}
		if (!InCommand.TargetActor->Implements<UInteractable>())
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[UGACloneTech::HandleTargetAccepted] TargetActor가 Interact 대상이 아닙니다")
			);
			EndAbility(this->CurrentSpecHandle, this->CurrentActorInfo, this->CurrentActivationInfo, true, true);
			return;
		}
		break;
	}
	default:
	{
		EndAbility(this->CurrentSpecHandle, this->CurrentActorInfo, this->CurrentActivationInfo, true, true);
		return;
	}
	}
	if (!this->CloneClass)
	{
		EndAbility(this->CurrentSpecHandle, this->CurrentActorInfo, this->CurrentActivationInfo, true, true);
		return ;
	}
	if (!CommitAbility(this->CurrentSpecHandle, this->CurrentActorInfo, this->CurrentActivationInfo))
	{
		EndAbility(this->CurrentSpecHandle, this->CurrentActorInfo, this->CurrentActivationInfo, true, true);
		return ;
	}

	const bool		bIsRightDir = InCommand.TargetLocation.X - Character->GetActorLocation().X > 0.0f;
	const FVector	CloneDir = bIsRightDir ? FVector::XAxisVector : -FVector::XAxisVector;
	const FVector	ClonePos = Character->GetActorLocation() + this->CloneDist * CloneDir;
	const FRotator	CloneRotate(0.0f, bIsRightDir ? 0.0f : 180.0f, 0.0f);

	// 기존 AdjustIfPossibleButDontSpawnIfColliding 스폰과 동일하게, 겹치면 위치를 보정하고 보정할 수 없으면 생성하지 않는다
	// (풀에서 꺼낸 액터는 텔레포트로 배치되므로 직접 검사)
	FVector	SpawnPos = ClonePos;
	if (!GetWorld()->FindTeleportSpot(this->CloneClass->GetDefaultObject<ACloneCharacter>(), SpawnPos, CloneRotate))
	{
		EndAbility(this->CurrentSpecHandle, this->CurrentActorInfo, this->CurrentActivationInfo, true, true);
		return ;
	}

	// 분신 풀이 등록되어 있지 않으면 일반 스폰된다
	UObjectPoolSubsystem*	PoolSubsystem = GetWorld()->GetSubsystem<UObjectPoolSubsystem>();
	if (PoolSubsystem)
	{
		this->SpawnedClone = PoolSubsystem->SpawnActor<ACloneCharacter>(
			this->CloneClass, FTransform(CloneRotate, SpawnPos), Character, Character);
	}
	if (!this->SpawnedClone.IsValid())
	{
		EndAbility(this->CurrentSpecHandle, this->CurrentActorInfo, this->CurrentActivationInfo, true, true);
		return ;
	}
	this->SpawnedClone->SetCommand(InCommand);
	this->SpawnedClone->OnCloneDespawned.AddDynamic(this, &UGACloneTech::HandleCloneDespawned);
	// 수명이 끝나면 ACloneCharacter::LifeSpanExpired에서 풀에 반환된다
	this->SpawnedClone->SetLifeSpan(FMath::Max(this->CloneLifeTime, 0.01f));

	// 체크포인트 리스폰 시 분신 제거 (HandleCloneDespawned에서 어빌리티가 정리됨)
	if (UCheckpointSubsystem* Subsystem = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
	{
		Subsystem->RegisterRuntimeSpawnedActor(this->SpawnedClone.Get());
	}
}

void UGACloneTech::HandleTargetCancelled()
{
	if (!IsActive())
	{
		return ;
	}
	this->TargetingTask = nullptr;
	SetAvatarCloneTargeting(false);
	EndAbility(this->CurrentSpecHandle, this->CurrentActorInfo, this->CurrentActivationInfo, true, true);
}

void UGACloneTech::SetAvatarCloneTargeting(bool bTargeting) const
{
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		Player->SetCloneTargeting(bTargeting);
	}
}

bool UGACloneTech::CheckCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo)
	{
		return (false);
	}
	if (!ActorInfo->AbilitySystemComponent.IsValid())
	{
		return (false);
	}
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return (false);
	}
	return (true);
}

void UGACloneTech::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (this->TargetingTask)
	{
		return ;
	}
	if (this->bIsSwaped)
	{
		return ;
	}
	ACharacter*	Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return ;
	}
	if (!this->SpawnedClone.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return ;
	}
	this->bIsSwaped = true;
	const FVector	PlayerLocation = Character->GetActorLocation();
	Character->SetActorLocation(this->SpawnedClone->GetActorLocation());
	this->SpawnedClone->SetActorLocation(PlayerLocation);
}

void UGACloneTech::HandleCloneDespawned(ACloneCharacter* DespawnedClone)
{
	if (!DespawnedClone)
	{
		return ;
	}
	DespawnedClone->OnCloneDespawned.RemoveDynamic(this, &UGACloneTech::HandleCloneDespawned);
	this->SpawnedClone.Reset();
	if (IsActive())
	{
		EndAbility(this->CurrentSpecHandle, this->CurrentActorInfo, this->CurrentActivationInfo, true, false);
	}
}

void UGACloneTech::OnRecallClone()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}
	// 남은 쿨타임 확인해 줄어들 쿨타임과 비교 후 적용
	const float RemainingCooldown = GetCooldownTimeRemaining();
	if (this->RecallCooldown < RemainingCooldown && CooldownEffectHandle.IsValid())
	{
		// -값이 들어가야 하는 것이 맞다
		const float	ReduceCoolTime = this->RecallCooldown - RemainingCooldown;
		
		ASC->ModifyActiveEffectStartTime(
			this->CooldownEffectHandle,
			ReduceCoolTime
		);
	}
	// 분신이 존재하면 Release 시키기
	if (this->SpawnedClone.IsValid())
	{
		UObjectPoolSubsystem::Release(this->SpawnedClone.Get());
	}
}

void UGACloneTech::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool	bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return ;
	}
	if (this->TargetingTask)
	{
		this->TargetingTask->OnAccepted.RemoveDynamic(this, &UGACloneTech::HandleTargetAccepted);
		this->TargetingTask->OnCancelled.RemoveDynamic(this, &UGACloneTech::HandleTargetCancelled);
		this->TargetingTask->EndTask();
		this->TargetingTask = nullptr;
	}
	SetAvatarCloneTargeting(false);
	if (ACloneCharacter* Clone = this->SpawnedClone.Get())
	{
		Clone->OnCloneDespawned.RemoveDynamic(this, &UGACloneTech::HandleCloneDespawned);
	}
	this->SpawnedClone.Reset();
	this->bIsSwaped = false;
	//this->CooldownEffectHandle = FActiveGameplayEffectHandle();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGACloneTech::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	this->CooldownEffectHandle = FActiveGameplayEffectHandle();

	UGameplayEffect*	CooldownGE = GetCooldownGameplayEffect();
	if (CooldownGE)
	{
		this->CooldownEffectHandle = ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, CooldownGE, GetAbilityLevel(Handle, ActorInfo));
	}
}
