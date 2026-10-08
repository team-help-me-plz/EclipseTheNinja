


#include "Widgets/StatBarWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

void UStatBarWidget::UpdateValue(float InValue, float InMaxValue)
{
	MaxValue = FMath::Max(0.f, InMaxValue);
	CurrentValue = FMath::Clamp(InValue, 0.f, MaxValue);
	TargetValue = MaxValue > 0.f ? CurrentValue / MaxValue : 0.f;
	UpdateText();
}

void UStatBarWidget::NativePreConstruct()
{
	// Bar마다 다른 색상을 가지도록
	Super::NativePreConstruct();
	if (ProgressBar)
	{
		ProgressBar->SetFillColorAndOpacity(BarColor);
		ProgressBar->SetPercent(TargetValue);
	}
	UpdateText();
}

void UStatBarWidget::NativeTick(const FGeometry & MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateBar(InDeltaTime);
}

void UStatBarWidget::UpdateBar(float InDeltaTime)
{
	if (ProgressBar)
	{
		const float NewPercent = FMath::FInterpTo(ProgressBar->GetPercent(), TargetValue, InDeltaTime, InterpSpeed);
		ProgressBar->SetPercent(NewPercent);
	}
}

void UStatBarWidget::UpdateText()
{
	if (CurrentValueText)
	{
		CurrentValueText->SetText(FText::AsNumber(FMath::RoundToInt(CurrentValue)));
	}

	if (MaxValueText)
	{
		MaxValueText->SetText(FText::AsNumber(FMath::RoundToInt(MaxValue)));
	}

}
