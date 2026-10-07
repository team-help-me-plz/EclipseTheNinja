// Fill out your copyright notice in the Description page of Project Settings.


#include "GeuyoonTest/CloneTechTestCharacter.h"

#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
//#include "Components/InputComponent.h"

ACloneTechTestCharacter::ACloneTechTestCharacter()
{
	this->AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
}

UAbilitySystemComponent* ACloneTechTestCharacter::GetAbilitySystemComponent() const
{
	return (this->AbilitySystemComponent);
}

void ACloneTechTestCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (IsValid(this->AbilitySystemComponent))
	{
		this->AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void ACloneTechTestCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(CloneTechAction, ETriggerEvent::Started, this, &ACloneTechTestCharacter::DoCloneTech);
	}
}

void ACloneTechTestCharacter::GiveInitAbility()
{
	if (!this->AbilitySystemComponent->AbilityActorInfo.IsValid())
	{
		this->AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
	for (const FAbilityInfo& AbilityInfo : this->AbilityClassArray)
	{
		if (!AbilityInfo.AbilityClass)
		{
			continue ;
		}
		if (FGameplayAbilitySpec* ExistingSpec = AbilitySystemComponent->FindAbilitySpecFromClass(AbilityInfo.AbilityClass))
		{
			ExistingSpec->InputID = AbilityInfo.InputID;
			this->AbilitySystemComponent->MarkAbilitySpecDirty(*ExistingSpec);
			this->AbilityHandles.Add(AbilityInfo.InputID, ExistingSpec->Handle);
			continue ;
		}
		FGameplayAbilitySpec				AbilitySpec(AbilityInfo.AbilityClass, 
														AbilityInfo.AbilityLevel, 
														AbilityInfo.InputID, this);
		const FGameplayAbilitySpecHandle	Handle = AbilitySystemComponent->GiveAbility(AbilitySpec);

		this->AbilityHandles.Add(AbilityInfo.InputID, Handle);
	}
}

void ACloneTechTestCharacter::OnLocalInputStart(int32 InID)
{
	if (this->AbilitySystemComponent)
	{
		this->AbilitySystemComponent->AbilityLocalInputPressed(InID);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("ASC 없음")
		);
	}
}

void ACloneTechTestCharacter::OnLocalInputCompleted(int32 InID)
{
	if (this->AbilitySystemComponent)
	{
		this->AbilitySystemComponent->AbilityLocalInputReleased(InID);
	}
}

void ACloneTechTestCharacter::DoCloneTech()
{
	UE_LOG(
		LogTemp,
		Log,
		TEXT("분신술")
	);
	this->OnLocalInputStart(this->CloneTechInputID);
}

void ACloneTechTestCharacter::DoInteract()
{
}
