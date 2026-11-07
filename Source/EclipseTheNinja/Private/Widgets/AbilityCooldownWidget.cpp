


#include "Widgets/AbilityCooldownWidget.h"
#include "Widgets/InputGlyphWidget.h"
#include "Character/PlayerCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "GAS/Ability/GACooldownBase.h"
#include "Character/BaseCharacter.h"
#include "Save/GameSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

void UAbilityCooldownWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// WBP에서 숨김으로 둔 경우에도 나타날 때는 보이도록 한다
	const ESlateVisibility DesignVisibility = GetVisibility();
	if (DesignVisibility != ESlateVisibility::Collapsed && DesignVisibility != ESlateVisibility::Hidden)
	{
		this->ShownVisibility = DesignVisibility;
	}
}

void UAbilityCooldownWidget::NativeDestruct()
{
	this->StopWaitingForGrant();

	Super::NativeDestruct();
}

void UAbilityCooldownWidget::SetAbilityVisible(bool bInVisible)
{
	SetVisibility(bInVisible ? this->ShownVisibility : ESlateVisibility::Collapsed);
}

void UAbilityCooldownWidget::StopWaitingForGrant()
{
	if (ABaseCharacter* Character = this->GrantWatchedCharacter.Get())
	{
		Character->OnAbilityGranted.Remove(this->AbilityGrantedHandle);
	}
	this->AbilityGrantedHandle.Reset();
	this->GrantWatchedCharacter.Reset();
}

void UAbilityCooldownWidget::HandleAbilityGranted(EAbilityInputID InInputID)
{
	if (InInputID != this->TargetAbilityInputID)
	{
		return;
	}
	ABaseCharacter* Character = this->GrantWatchedCharacter.Get();
	this->StopWaitingForGrant();
	if (!Character)
	{
		return;
	}
	// 부여된 어빌리티 인스턴스에 다시 바인딩한다 (파생 위젯의 추가 바인딩 포함)
	this->InitWidget(Character);

	// 이어하기로 저장된 해금을 복원할 때는 획득 연출 없이 바로 보이게만 한다
	const UGameInstance* GameInstance = GetGameInstance();
	const UGameSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UGameSaveSubsystem>() : nullptr;
	if (Save && Save->IsApplyingRestore())
	{
		return;
	}
	if (Character->FindAbilitySpecByInputID(this->TargetAbilityInputID))
	{
		this->BP_OnAbilityGranted();
	}
}

void	UAbilityCooldownWidget::InitWidget(APawn* InPawn)
{
	this->UnbindAbility();
	this->StopWaitingForGrant();

	if (const APlayerCharacter* Player = Cast<APlayerCharacter>(InPawn))
	{
		const UInputAction* Action = Player->GetAbilityInputAction(TargetAbilityInputID);
		if (AbilityKeyGlyph) AbilityKeyGlyph->SetInputAction(Action);
		else AbilityKeyGlyph = InstallKeyGlyph(TEXT("AbilityKeyText"), TEXT("AbilityKeyBackground"), Action);
	}
	if (!InPawn)
	{
		return ;
	}
	if (this->bHideUntilGranted && this->TargetAbilityInputID != EAbilityInputID::None)
	{
		ABaseCharacter* Character = Cast<ABaseCharacter>(InPawn);
		const bool bHasAbility = Character && Character->FindAbilitySpecByInputID(this->TargetAbilityInputID);
		this->SetAbilityVisible(bHasAbility);
		if (Character && !bHasAbility)
		{
			// 아직 없는 어빌리티는 숨긴 채로 획득을 기다린다
			this->GrantWatchedCharacter = Character;
			this->AbilityGrantedHandle = Character->OnAbilityGranted.AddUObject(this, &UAbilityCooldownWidget::HandleAbilityGranted);
			return ;
		}
	}
	else
	{
		this->SetAbilityVisible(true);
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

UInputGlyphWidget* UAbilityCooldownWidget::InstallKeyGlyph(FName TextName, FName BackgroundName, const UInputAction* Action)
{
	if (!Action) return nullptr;
	if (!WidgetTree) return nullptr;

	UWidget* Text = WidgetTree->FindWidget(TextName);
	if (!Text) return nullptr;

	UOverlay* Parent = Cast<UOverlay>(Text->GetParent());
	if (!Parent) return nullptr;

	UInputGlyphWidget* Glyph = CreateWidget<UInputGlyphWidget>(this);
	if (!Glyph) return nullptr;

	// 기존 슬롯의 순서를 유지해 사용 불가/분신 교대 표시가 키 위에 그려지게 한다.
	if (!Parent->ReplaceOverlayChildAt(Parent->GetChildIndex(Text), Glyph)) return nullptr;
	UOverlaySlot* GlyphSlot = CastChecked<UOverlaySlot>(Glyph->Slot);
	GlyphSlot->SetHorizontalAlignment(HAlign_Fill);
	GlyphSlot->SetVerticalAlignment(VAlign_Fill);
	Glyph->SetInputAction(Action);
	Text->SetVisibility(ESlateVisibility::Collapsed);
	if (UWidget* Background = WidgetTree->FindWidget(BackgroundName)) Background->SetVisibility(ESlateVisibility::Collapsed);
	return Glyph;
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
