// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/GACloneTech.h"
#include "Clone/CloneCharacter.h"

#include "GameFramework/Character.h"

void UGACloneTech::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UE_LOG(
		LogTemp,
		Log,
		TEXT("분신 소환할까용")
	);
	// 어빌리티 사용 불가능 상태
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("못써용")
		);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// 잘못된 대상이 어빌리티 사용
	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("이상한 애가 불렀어용")
		);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UE_LOG(
		LogTemp,
		Log,
		TEXT("분신 소환해용")
	);
	const FVector	CharacterDir = Character->GetActorForwardVector();
	const bool		bIsRightDir = CharacterDir.X > 0;
	const FVector	CloneDir = bIsRightDir ? FVector(1.0f, 0.0f, 0.0f) : FVector(-1.0f, 0.0f, 0.0f);
	const FVector	ClonePos = Character->GetActorLocation() + this->CloneDist * CloneDir;
	const FRotator	CloneRotate(0.0f, bIsRightDir ? 0.0f : 180.0f, 0.0f);
	FActorSpawnParameters Params;
	Params.Owner = Character;
	Params.Instigator = Character;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	ACloneCharacter* Clone = GetWorld()->SpawnActor<ACloneCharacter>(
		CloneClass,
		ClonePos,
		CloneRotate,
		Params
	);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, Clone == nullptr);
}

bool UGACloneTech::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	//UGameplayEffect* CostGE = GetCostGameplayEffect();
	//if (!CostGE)	return (true);

	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)	return (false);

	return (true);
}

void UGACloneTech::OnTargetingConfirmed(const FTargetingResult& Result)
{

}

void UGACloneTech::OnTargetingCancelled()
{

}