


#include "Widgets/CloneTechCooldownWidget.h"
#include "GAS/Ability/GACloneTech.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Character/PlayerCharacter.h"
#include "Widgets/InputGlyphWidget.h"
#include "Character/PlayerCharacterController.h"

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

void UCloneTechCooldownWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 게임패드를 연결하거나 뺀 것을 바로 반영한다
	RefreshGamepadKeyIcon();
}

void UCloneTechCooldownWidget::SetAbilityKeyPromptVisible(bool bInVisible)
{
	this->bAbilityKeyPromptVisible = bInVisible;

	// 키 그림(AbilityKeyGlyph)이 글자를 대신하고 있으면 글자는 계속 숨겨 둔다
	if (AbilityKeyGlyph)
	{
		AbilityKeyGlyph->SetVisibility(bInVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		this->AbilityKeyText->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		this->AbilityKeyText->SetVisibility(bInVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	RefreshGamepadKeyIcon();
}

void UCloneTechCooldownWidget::RefreshGamepadKeyIcon()
{
	UWidget* Icon = GetWidgetFromName(TEXT("AbilityGamepadKeyIcon"));
	if (!Icon)
	{
		return;
	}

	// 숨길 때는 항상 숨긴다
	if (!this->bAbilityKeyPromptVisible)
	{
		Icon->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 이 아이콘을 InputPromptBindings가 맡고 있으면 부모의 갱신(게임패드일 때만, 아이콘 그림이 있을 때만 표시)에 맡긴다
	const bool bBound = InputPromptBindings.ContainsByPredicate([](const FCooldownInputPromptBinding& Binding)
		{ return Binding.GamepadIconWidgetName == TEXT("AbilityGamepadKeyIcon"); });
	if (bBound)
	{
		return;
	}

	const APlayerCharacterController* Controller = Cast<APlayerCharacterController>(GetOwningPlayer());
	const bool bGamepad = Controller && Controller->IsUsingGamepad();
	Icon->SetVisibility(bGamepad ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UCloneTechCooldownWidget::CloneSpawnedUpdate()
{
	// 분신을 소환하면 같은 키가 "교환"이 된다: 교환 그림을 보이고 키 안내는 숨긴다
	this->CloneTechSwapImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	this->CloneTechSwapRecallUsable->SetVisibility(ESlateVisibility::Collapsed);
	SetAbilityKeyPromptVisible(false);
}

void UCloneTechCooldownWidget::CloneSwapCannotUpdate()
{
	const UGACloneTech* Ability = Cast<UGACloneTech>(ObservedAbility.Get());
	bool bHasClone = false;
	if (Ability) bHasClone = Ability->HasSpawnedClone();

	// 교환했거나 분신이 사라지면 더 교환할 수 없다: 교환 그림을 숨기고 키 안내를 다시 보인다.
	// 분신이 남아 있는 동안(교환 후)에는 사용 불가 표시를 남긴다.
	CloneTechSwapImage->SetVisibility(ESlateVisibility::Collapsed);
	CloneTechSwapRecallUsable->SetVisibility(bHasClone ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	CloneTechRecallUsable->SetVisibility(bHasClone ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	SetAbilityKeyPromptVisible(true);
}

void UCloneTechCooldownWidget::StartCooldown()
{
	Super::StartCooldown();

	// 쿨타임은 분신 소환 직전에 시작된다. 소환되면 CloneSpawnedUpdate가 교환 그림으로 바꾼다
	this->CloneTechSwapImage->SetVisibility(ESlateVisibility::Collapsed);
	this->CloneTechRecallUsable->SetVisibility(ESlateVisibility::Collapsed);
	SetAbilityKeyPromptVisible(true);
}

void UCloneTechCooldownWidget::EndCooldown()
{
	Super::EndCooldown();

	this->CloneTechSwapImage->SetVisibility(ESlateVisibility::Collapsed);
	this->CloneTechSwapRecallUsable->SetVisibility(ESlateVisibility::Collapsed);
	this->CloneTechRecallUsable->SetVisibility(ESlateVisibility::Visible);
	SetAbilityKeyPromptVisible(true);
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
