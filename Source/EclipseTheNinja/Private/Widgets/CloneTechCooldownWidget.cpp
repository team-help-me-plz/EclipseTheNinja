


#include "Widgets/CloneTechCooldownWidget.h"
#include "GAS/Ability/GACloneTech.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Character/PlayerCharacter.h"
#include "Widgets/InputGlyphWidget.h"

void UCloneTechCooldownWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCloneTechCooldownWidget::InitWidget(APawn* InPawn)
{
	Super::InitWidget(InPawn);
	if (const APlayerCharacter* Player = Cast<APlayerCharacter>(InPawn))
	{
		if (RecallKeyGlyph) RecallKeyGlyph->SetInputAction(Player->GetCloneRecallInputAction());
		else RecallKeyGlyph = InstallKeyGlyph(TEXT("CloneTechRecallKeyText"), TEXT("CloneTechRecallKeyBackground"), Player->GetCloneRecallInputAction());
	}

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
	this->CloneTechSwapImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	this->CloneTechSwapRecallUsable->SetVisibility(ESlateVisibility::Collapsed);
}

void UCloneTechCooldownWidget::CloneSwapCannotUpdate()
{
	const UGACloneTech* Ability = Cast<UGACloneTech>(ObservedAbility.Get());
	bool bHasClone = false;
	if (Ability) bHasClone = Ability->HasSpawnedClone();

	// 교대 후에는 사용 불가 표시를 남기고, 회수·소멸 후에는 교환 그림 자체를 숨긴다.
	CloneTechSwapImage->SetVisibility(bHasClone ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	CloneTechSwapRecallUsable->SetVisibility(bHasClone ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	CloneTechRecallUsable->SetVisibility(bHasClone ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UCloneTechCooldownWidget::StartCooldown()
{
	Super::StartCooldown();

	// 소환 중에도 같은 키로 교대할 수 있으므로 R키 안내를 유지한다.
	this->AbilityKeyText->SetVisibility(AbilityKeyGlyph ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (AbilityKeyGlyph) AbilityKeyGlyph->SetVisibility(ESlateVisibility::HitTestInvisible);
	this->CloneTechSwapImage->SetVisibility(ESlateVisibility::Collapsed);
	this->CloneTechRecallUsable->SetVisibility(ESlateVisibility::Collapsed);
}

void UCloneTechCooldownWidget::EndCooldown()
{
	Super::EndCooldown();

	this->AbilityKeyText->SetVisibility(AbilityKeyGlyph ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (AbilityKeyGlyph) AbilityKeyGlyph->SetVisibility(ESlateVisibility::HitTestInvisible);
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
	if (!CloneTechAbility)
	{
		this->ObservedAbility.Reset();
		return;
	}
	CloneTechAbility->OnCloneSpawned.RemoveAll(this);
	CloneTechAbility->OnCloneCannotSwap.RemoveAll(this);
	this->ObservedAbility.Reset();
}
