#include "Widgets/ElementalFireCooldownWidget.h"
#include "GAS/Ability/GAElementalFire.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

void UElementalFireCooldownWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (this->FireActiveImage)
	{
		this->FireActiveMaterial = this->FireActiveImage->GetDynamicMaterial();
	}
	this->FireDisplayOptions.SetMinimumFractionalDigits(0);
	this->FireDisplayOptions.SetMaximumFractionalDigits(1);

	this->EndFireDuration();
}

void UElementalFireCooldownWidget::InitWidget(APawn* InPawn)
{
	Super::InitWidget(InPawn);

	UGAElementalFire*	FireAbility = Cast<UGAElementalFire>(this->ObservedAbility.Get());
	if (!FireAbility)
	{
		this->UnbindAbility();
		return ;
	}
	FireAbility->OnFireElementStarted.AddUObject(this, &UElementalFireCooldownWidget::StartFireDuration);
	FireAbility->OnFireElementEnded.AddUObject(this, &UElementalFireCooldownWidget::EndFireDuration);

	// 위젯이 다시 초기화될 때 이미 불이 붙어 있으면 바로 표시한다.
	float	Remaining = 0.f;
	float	Duration = 0.f;
	if (FireAbility->GetFireElementTime(Remaining, Duration))
	{
		this->StartFireDuration();
	}
}

void UElementalFireCooldownWidget::StartFireDuration()
{
	this->FireActiveImage->SetVisibility(ESlateVisibility::Visible);
	this->CooldownText->SetVisibility(ESlateVisibility::Visible);
	this->CooldownMaterial->SetScalarParameterValue(TEXT("Progress"), 0.f);
	this->CooldownImage->SetVisibility(ESlateVisibility::Visible);
	GetWorld()->GetTimerManager().SetTimer(
		this->FireDurationUpdateTimer,
		this,
		&UElementalFireCooldownWidget::UpdateFireDuration,
		this->CooldownUpdateInterval,
		true,
		0.f
	);
}

void UElementalFireCooldownWidget::EndFireDuration()
{
	this->FireActiveImage->SetVisibility(ESlateVisibility::Collapsed);
	this->FireImage->SetVisibility(ESlateVisibility::Collapsed);
	//this->FireActiveText->SetVisibility(ESlateVisibility::Collapsed);
	GetWorld()->GetTimerManager().ClearTimer(this->FireDurationUpdateTimer);
}

void UElementalFireCooldownWidget::UpdateFireDuration()
{
	float	Remaining = 0.f;
	float	Duration = 0.f;

	const UGAElementalFire*	FireAbility = Cast<UGAElementalFire>(this->ObservedAbility.Get());
	if (!FireAbility || !FireAbility->GetFireElementTime(Remaining, Duration) || Duration <= 0.f)
	{
		this->EndFireDuration();
		return ;
	}

	// 불이 줄어드는 느낌이 나도록 남은 비율(1 > 0)을 넘긴다.
	if (this->FireActiveMaterial)
	{
		this->FireActiveMaterial->SetScalarParameterValue(TEXT("Progress"), 1.f - Remaining / Duration);
	}

	// 기존 쿨타임 표시와 같은 규칙: 0.7초 초과는 정수, 이하는 소수 첫째 자리
	float	DisplayTime = 0.f;
	if (Remaining > 0.7f)
	{
		DisplayTime = FMath::CeilToFloat(Remaining);
	}
	else
	{
		DisplayTime = FMath::CeilToFloat(Remaining * 10.f) / 10.f;
	}
	this->CooldownText->SetText(FText::AsNumber(DisplayTime, &this->FireDisplayOptions));
}

void UElementalFireCooldownWidget::EndCooldown()
{
	Super::EndCooldown();

	this->FireImage->SetVisibility(ESlateVisibility::Visible);
}

void UElementalFireCooldownWidget::UnbindAbility()
{
	this->EndFireDuration();

	if (UGAElementalFire* FireAbility = Cast<UGAElementalFire>(this->ObservedAbility.Get()))
	{
		FireAbility->OnFireElementStarted.RemoveAll(this);
		FireAbility->OnFireElementEnded.RemoveAll(this);
	}

	Super::UnbindAbility();
}
