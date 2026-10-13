// Fill out your copyright notice in the Description page of Project Settings.


#include "GeuyoonTest/CloneTechTestCharacter.h"
#include "Interface/Interactable.h"

#include "Components/SphereComponent.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"

ACloneTechTestCharacter::ACloneTechTestCharacter()
{
	this->InteractCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractCollision"));
	this->InteractCollision->SetSphereRadius(this->InteractRadius);
	this->InteractCollision->SetupAttachment(GetRootComponent());

	this->AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
}

UAbilitySystemComponent* ACloneTechTestCharacter::GetAbilitySystemComponent() const
{
	return (this->AbilitySystemComponent);
}

void ACloneTechTestCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	this->InteractCollision->OnComponentBeginOverlap.AddDynamic(this, &ACloneTechTestCharacter::OnInteractBeginOverlap);
	this->InteractCollision->OnComponentEndOverlap.AddDynamic(this, &ACloneTechTestCharacter::OnInteractEndOverlap);
}

void ACloneTechTestCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (IsValid(this->AbilitySystemComponent))
	{
		this->AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
	this->GiveInitAbility();
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
	this->OnLocalInputStart(this->CloneTechInputID);
}

void ACloneTechTestCharacter::DoInteract()
{
    if (this->OverlappedInteractActor.Num())
    {
		UE_LOG(LogTemp, Log, TEXT("상호작용 시작"));
		IInteractable::Execute_Interact(this->OverlappedInteractActor[this->OverlappedInteractActorTargetIndex], this);
    }
}

void ACloneTechTestCharacter::OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    UE_LOG(LogTemp, Log, TEXT("뭔가 overlap됨"));
    if (OtherActor && OtherActor->Implements<UInteractable>() && IInteractable::Execute_bIsInteractReady(OtherActor))
    {
        UE_LOG(LogTemp, Log, TEXT("[%s]가 overlap됨"), *OtherActor->GetName());
        this->OverlappedInteractActor.AddUnique(OtherActor);
        this->OverlappedInteractActorTargetIndex = 0;
    }
}

void ACloneTechTestCharacter::OnInteractEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (OtherActor && OtherActor->Implements<UInteractable>() && IInteractable::Execute_bIsInteractReady(OtherActor))
    {
        UE_LOG(LogTemp, Log, TEXT("[%s]가 overlap 풀림"), *OtherActor->GetName());
        this->OverlappedInteractActor.Remove(OtherActor);
        this->OverlappedInteractActorTargetIndex = 0;
    }
}
