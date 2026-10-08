// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/GACloneTech.h"
#include "Clone/CloneCharacter.h"

#include "GameFramework/Character.h"

UGACloneTech::UGACloneTech()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGACloneTech::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 어빌리티 사용 불가능 상태
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// 잘못된 대상이 어빌리티 사용
	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// 내부 값 초기화
	this->bIsSwaped = false;
	this->SpawnedClone.Reset();

	// 분신 이동 관련 값 초기화
	const FVector	CharacterDir = Character->GetActorForwardVector();
	const bool		bIsRightDir = CharacterDir.X > 0;
	const FVector	CloneDir = bIsRightDir ? FVector(1.0f, 0.0f, 0.0f) : FVector(-1.0f, 0.0f, 0.0f);
	const FVector	ClonePos = Character->GetActorLocation() + this->CloneDist * CloneDir;
	const FRotator	CloneRotate(0.0f, bIsRightDir ? 0.0f : 180.0f, 0.0f);
	FActorSpawnParameters Params;
	Params.Owner = Character;
	Params.Instigator = Character;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	// 분신 소환
	this->SpawnedClone = GetWorld()->SpawnActor<ACloneCharacter>(
		CloneClass,
		ClonePos,
		CloneRotate,
		Params
	);
	if (!this->SpawnedClone.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
	this->SpawnedClone.Get()->OnDestroyed.AddDynamic(this, &UGACloneTech::HandleCloneDestroyed);
}

bool UGACloneTech::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	//UGameplayEffect* CostGE = GetCostGameplayEffect();
	//if (!CostGE)	return (true);

	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)	return (false);

	return (true);
}

void UGACloneTech::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
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
	if (this->bIsSwaped)
	{
		return ;
	}
	this->bIsSwaped = true;
	const FVector	PlayerLocation = Character->GetActorLocation();
	const FVector	CloneLocation = this->SpawnedClone.Get()->GetActorLocation();

	Character->SetActorLocation(CloneLocation);
	this->SpawnedClone.Get()->SetActorLocation(PlayerLocation);
	//EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGACloneTech::HandleCloneDestroyed(AActor* DestroyedActor)
{
	if (!DestroyedActor)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[UGACloneTech::HandleCloneDestroyed] HandleCloneDestroyed에 nullptr이 들어왔습니다.")
		);
		return ;
	}
	DestroyedActor->OnDestroyed.RemoveDynamic(this, &UGACloneTech::HandleCloneDestroyed);
	this->bIsSwaped = false;
	this->SpawnedClone.Reset();
	if (!IsActive())
	{
		return ;
	}
	EndAbility(
		CurrentSpecHandle,
		CurrentActorInfo,
		CurrentActivationInfo,
		true,
		false
	);
}
