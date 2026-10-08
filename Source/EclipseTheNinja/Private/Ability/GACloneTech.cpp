#include "Ability/GACloneTech.h"
#include "Clone/CloneCharacter.h"
#include "ProjComponent/AbilityTask_WaitCloneTargeting.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Interface/Interactable.h"

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
		this->CancelInputAction.Get(), this->TargetingMappingPriority, this->TargetingTimeDilation);
	if (!this->TargetingTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return ;
	}

	this->TargetingTask->OnAccepted.AddDynamic(this, &UGACloneTech::HandleTargetAccepted);
	this->TargetingTask->OnCancelled.AddDynamic(this, &UGACloneTech::HandleTargetCancelled);
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

	FActorSpawnParameters	Params;
	Params.Owner = Character;
	Params.Instigator = Character;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	this->SpawnedClone = GetWorld()->SpawnActor<ACloneCharacter>(this->CloneClass, ClonePos, CloneRotate, Params);
	if (!this->SpawnedClone.IsValid())
	{
		EndAbility(this->CurrentSpecHandle, this->CurrentActorInfo, this->CurrentActivationInfo, true, true);
		return ;
	}
	this->SpawnedClone->SetCommand(InCommand);
	this->SpawnedClone->OnDestroyed.AddDynamic(this, &UGACloneTech::HandleCloneDestroyed);
	this->SpawnedClone->SetLifeSpan(FMath::Max(this->CloneLifeTime, 0.01f));
}

void UGACloneTech::HandleTargetCancelled()
{
	if (!IsActive())
	{
		return ;
	}
	this->TargetingTask = nullptr;
	EndAbility(this->CurrentSpecHandle, this->CurrentActorInfo, this->CurrentActivationInfo, true, true);
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

void UGACloneTech::HandleCloneDestroyed(AActor* DestroyedActor)
{
	if (!DestroyedActor)
	{
		return ;
	}
	DestroyedActor->OnDestroyed.RemoveDynamic(this, &UGACloneTech::HandleCloneDestroyed);
	this->SpawnedClone.Reset();
	if (IsActive())
	{
		EndAbility(this->CurrentSpecHandle, this->CurrentActorInfo, this->CurrentActivationInfo, true, false);
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
	if (ACloneCharacter* Clone = this->SpawnedClone.Get())
	{
		Clone->OnDestroyed.RemoveDynamic(this, &UGACloneTech::HandleCloneDestroyed);
	}
	this->SpawnedClone.Reset();
	this->bIsSwaped = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
