


#include "Widgets/AbilityCooldownWidget.h"
#include "GAS/Ability/GACooldownBase.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

void	UAbilityCooldownWidget::InitWidget(APawn* InPawn)
{
	this->UnbindAbility();
	if (!InPawn)
	{
		return ;
	}
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InPawn);
	if (!ASI)
	{
		return;
	}
	UAbilitySystemComponent* AbilitySystemComp = ASI->GetAbilitySystemComponent();
	if (!AbilitySystemComp)
	{
		return;
	}
	FGameplayAbilitySpec* Spec = AbilitySystemComp->FindAbilitySpecFromInputID(ToAbilityInputID(this->TargetAbilityInputID));
	if (!Spec)
	{
		return ;
	}
	UGACooldownBase*	Ability = Cast<UGACooldownBase>(Spec->GetPrimaryInstance());
	if (!Ability)
	{
		return ;
	}
	Ability->OnAbilityCooldownStart.AddUObject(this, &UAbilityCooldownWidget::StartCooldown);
	this->ObservedAbility = Ability;
}

void UAbilityCooldownWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (this->CooldownImage)
	{
		this->CooldownMaterial = this->CooldownImage->GetDynamicMaterial();
		this->DisplayOptions.SetMinimumFractionalDigits(0);
		this->DisplayOptions.SetMaximumFractionalDigits(1);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[UAbilityCooldownWidget::UAbilityCooldownWidget] %s의 CooldownImage가 nullptr입니다"),
			*GetName()
		)
	}
}

void UAbilityCooldownWidget::StartCooldown()
{
	if (this->CooldownMaterial)
	{
		this->CooldownText->SetVisibility(ESlateVisibility::Visible);
		this->CooldownImage->SetVisibility(ESlateVisibility::Visible);
		GetWorld()->GetTimerManager().SetTimer(
			this->CooldownUpdateTimer,
			this,
			&UAbilityCooldownWidget::UpdateCooldown,
			this->CooldownUpdateInterval,
			true,
			0.f
		);
	}
}

void UAbilityCooldownWidget::EndCooldown()
{
	this->CooldownText->SetVisibility(ESlateVisibility::Collapsed);
	this->CooldownImage->SetVisibility(ESlateVisibility::Collapsed);
	GetWorld()->GetTimerManager().ClearTimer(this->CooldownUpdateTimer);
}

void UAbilityCooldownWidget::UpdateCooldown()
{
	float	Remainig = 0.f;
	float	Duration = 0.f;

	if (this->ObservedAbility.Get())
	{
		this->ObservedAbility.Get()->GetCooldown(Remainig, Duration);
		this->UpdateWidget(Remainig, Duration);
	}
	else
	{
		this->EndCooldown();
	}
}

void UAbilityCooldownWidget::UpdateWidget(const float InRemainig, const float InDuration)
{
	if (InRemainig == 0.f)
	{
		this->EndCooldown();

	}
	const float	MaterialProgress = 1.f - InRemainig / InDuration;
	
	if (this->CooldownMaterial)
	{
		this->CooldownMaterial->SetScalarParameterValue(TEXT("Progress"), MaterialProgress);
	}
	float	DisplayTime = 0.f;

	if (InRemainig > 0.7f)
	{
		DisplayTime = FMath::CeilToFloat(InRemainig);
	}
	else
	{
		DisplayTime = FMath::CeilToFloat(InRemainig * 10.f) / 10.f;
	}
	this->CooldownText->SetText(FText::AsNumber(DisplayTime, &this->DisplayOptions));
}

void UAbilityCooldownWidget::UnbindAbility()
{
	this->EndCooldown();

	UGACooldownBase* Ability = this->ObservedAbility.Get();
	if (!Ability)
	{
		return ;
	}
	Ability->OnAbilityCooldownStart.RemoveAll(this);
	this->ObservedAbility.Reset();
}
