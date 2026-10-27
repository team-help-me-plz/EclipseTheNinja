


#include "Widgets/CloneTechCooldownWidget.h"
#include "GAS/Ability/GACloneTech.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UCloneTechCooldownWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCloneTechCooldownWidget::InitWidget(APawn* InPawn)
{
	Super::InitWidget(InPawn);

	UGACooldownBase*	Ability = this->ObservedAbility.Get();
	if (!Ability)
	{
		return ;
	}
	UGACloneTech*	CloneTechAbility = Cast<UGACloneTech>(Ability);
	if (!CloneTechAbility)
	{
		this->UnbindAbility();
		return ;
	}
	CloneTechAbility->OnCloneSpawned.AddUObject(this, &UCloneTechCooldownWidget::CloneSpawnedUpdate);
	CloneTechAbility->OnCloneCannotSwap.AddUObject(this, &UCloneTechCooldownWidget::CloneSwapCannotUpdate);
}

void UCloneTechCooldownWidget::CloneSpawnedUpdate()
{
	this->CloneTechSwapRecallUsable->SetVisibility(ESlateVisibility::Collapsed);
}

void UCloneTechCooldownWidget::CloneSwapCannotUpdate()
{
	this->CloneTechSwapRecallUsable->SetVisibility(ESlateVisibility::Visible);
}

void UCloneTechCooldownWidget::StartCooldown()
{
	Super::StartCooldown();

	this->AbilityKeyText->SetVisibility(ESlateVisibility::Collapsed);
	this->CloneTechSwapImage->SetVisibility(ESlateVisibility::Visible);
	this->CloneTechRecallUsable->SetVisibility(ESlateVisibility::Collapsed);
}

void UCloneTechCooldownWidget::EndCooldown()
{
	Super::EndCooldown();

	this->AbilityKeyText->SetVisibility(ESlateVisibility::Visible);
	this->CloneTechSwapImage->SetVisibility(ESlateVisibility::Collapsed);
	this->CloneTechSwapRecallUsable->SetVisibility(ESlateVisibility::Collapsed);
	this->CloneTechRecallUsable->SetVisibility(ESlateVisibility::Visible);
}

void UCloneTechCooldownWidget::UnbindAbility()
{
	this->EndCooldown();

	UGACooldownBase* Ability = this->ObservedAbility.Get();
	if (!Ability)
	{
		return;
	}
	Ability->OnAbilityCooldownStart.RemoveAll(this);

	UGACloneTech* CloneTechAbility = Cast<UGACloneTech>(Ability);
	CloneTechAbility->OnCloneSpawned.RemoveAll(this);
	CloneTechAbility->OnCloneCannotSwap.RemoveAll(this);
	this->ObservedAbility.Reset();
}
