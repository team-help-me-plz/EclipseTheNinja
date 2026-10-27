

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NoiseRingWidget.generated.h"

class UImage;

DECLARE_MULTICAST_DELEGATE(FOnNoiseWaveFinished);

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UNoiseRingWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void	PlayAnim();
	// 세기(0~1)에 따라 링의 채도와 불투명도를 정한다. 애니메이션은 Render Opacity로 이 위에 곱해진다.
	void	SetStrength(float InStrength);
	//void	
	FOnNoiseWaveFinished	OnNoiseWaveFinished;
protected:
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;

	UPROPERTY(EditDefaultsOnly, Category = "Noise")
	FLinearColor RingColor = FLinearColor(0.0f, 0.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float	MinSaturation = 0.15f;

	/** 세기가 0일 때의 불투명도 */
	UPROPERTY(EditDefaultsOnly, Category = "Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float	MinOpacity = 0.2f;

	/** 세기가 1일 때의 불투명도 */
	UPROPERTY(EditDefaultsOnly, Category = "Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float	MaxOpacity = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage>	NoiseRangeFill = nullptr;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation>	NoiseWave = nullptr;
};
