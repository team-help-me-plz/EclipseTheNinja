// Fill out your copyright notice in the Description page of Project Settings.


#include "GeuyoonTest/CloneTechTestCharacter.h"
#include "Interface/Interactable.h"

#include "Components/BoxComponent.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Engine/TargetPoint.h"

ACloneTechTestCharacter::ACloneTechTestCharacter()
{
	this->InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractCollision"));
	//this->InteractCollision->SetSphereRadius(this->InteractRadius);
	this->InteractBox->SetBoxExtent(this->InteractBoxSize);
	this->InteractBox->SetupAttachment(GetRootComponent());

	this->AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
}

UAbilitySystemComponent* ACloneTechTestCharacter::GetAbilitySystemComponent() const
{
	return (this->AbilitySystemComponent);
}

void ACloneTechTestCharacter::RegisterSafePoint_Implementation(const ATargetPoint* InSafePoint)
{
	this->SafePointLocation = InSafePoint->GetActorLocation();
	this->SafePointRotate = InSafePoint->GetActorRotation();
}

void ACloneTechTestCharacter::ReturnToSafePoint_Implementation()
{
	if (this->SafePointLocation.IsNearlyZero())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[ACloneTechTestCharacter::ReturnToSafePoint] SafePoint가 등록되지 않았습니다.")
		);
		return ;
	}
	// 데미지 입는 애니메이션 + 페이드 아웃
	// 데미지 입는 애니메이션 끝나면 부활하는 애니메이션 + 페이드 인
	this->SetActorLocation(this->SafePointLocation);
	this->SetActorRotation(this->SafePointRotate);
}

void ACloneTechTestCharacter::ResetToInitial_Implementation()
{
	if (!IsValid(this->AbilitySystemComponent))
	{
		return ;
	}
	this->AbilitySystemComponent->CancelAllAbilities();

	FGameplayTagContainer 	CooldownTags;
	CooldownTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Test.GA.Cooldown.CloneTech")));

	this->AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(CooldownTags);
}

void ACloneTechTestCharacter::OnAfterReset_Implementation()
{
}

void ACloneTechTestCharacter::BeginPlay()
{
	Super::BeginPlay();

	this->InteractBox->SetAbsolute(false, true, true);
}

void ACloneTechTestCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	this->InteractBox->OnComponentBeginOverlap.AddDynamic(this, &ACloneTechTestCharacter::OnInteractBeginOverlap);
	this->InteractBox->OnComponentEndOverlap.AddDynamic(this, &ACloneTechTestCharacter::OnInteractEndOverlap);
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
		const int32 InputID = ToAbilityInputID(AbilityInfo.InputID);
		if (FGameplayAbilitySpec* ExistingSpec = AbilitySystemComponent->FindAbilitySpecFromClass(AbilityInfo.AbilityClass))
		{
			ExistingSpec->InputID = InputID;
			this->AbilitySystemComponent->MarkAbilitySpecDirty(*ExistingSpec);
			this->AbilityHandles.Add(InputID, ExistingSpec->Handle);
			continue ;
		}
		FGameplayAbilitySpec				AbilitySpec(AbilityInfo.AbilityClass,
														AbilityInfo.AbilityLevel,
														InputID, this);
		const FGameplayAbilitySpecHandle	Handle = AbilitySystemComponent->GiveAbility(AbilitySpec);

		this->AbilityHandles.Add(InputID, Handle);
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
		IInteractable::Execute_Interact(this->OverlappedInteractActor[this->OverlappedInteractActorTargetIndex], this);
    }
}

void ACloneTechTestCharacter::OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor->Implements<UInteractable>() && IInteractable::Execute_bIsInteractReady(OtherActor))
    {
        this->OverlappedInteractActor.AddUnique(OtherActor);
        this->OverlappedInteractActorTargetIndex = 0;
    }
}

void ACloneTechTestCharacter::OnInteractEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (OtherActor && OtherActor->Implements<UInteractable>() && this->OverlappedInteractActor.Find(OtherActor) != INDEX_NONE)
    {
        this->OverlappedInteractActor.Remove(OtherActor);
        this->OverlappedInteractActorTargetIndex = 0;
    }
}
