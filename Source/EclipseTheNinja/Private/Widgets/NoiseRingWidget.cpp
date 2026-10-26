


#include "Widgets/NoiseRingWidget.h"
#include "Components/Image.h"

void UNoiseRingWidget::PlayAnim()
{
	PlayAnimation(this->NoiseWave);
	//NoiseWave.Get()->
}

void UNoiseRingWidget::SetStrength(float InStrength)
{
	const float Strength = FMath::Clamp(InStrength, 0.0f, 1.0f);

	// LinearRGBToHSV 결과는 R=색상, G=채도, B=명도
	FLinearColor HSV = RingColor.LinearRGBToHSV();
	HSV.G *= FMath::Lerp(MinSaturation, 1.0f, Strength);

	FLinearColor Color = HSV.HSVToLinearRGB();
	Color.A = FMath::Lerp(MinOpacity, MaxOpacity, Strength);
	this->NoiseRangeFill->SetColorAndOpacity(Color);
}

void UNoiseRingWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);
	if (Animation == this->NoiseWave)
	{
		this->OnNoiseWaveFinished.Broadcast();
	}
}
