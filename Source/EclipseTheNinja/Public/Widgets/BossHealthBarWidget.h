#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossHealthBarWidget.generated.h"

class ABossEnemy;
class UProgressBar;
class UTextBlock;
class UImage;
class UMaterialInstanceDynamic;

/** Presentation logic only; layout, portrait, and threshold markers live in WBP_BossHealthBar. */
UCLASS()
class ECLIPSETHENINJA_API UBossHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitBoss(ABossEnemy* InBoss, const FText& InBossName);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BossNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ShieldStatusText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> EnergyLinkTimerImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> EnergyLinkTimerBackground;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Appearance")
	FLinearColor HealthColor = FLinearColor(0.65f, 0.025f, 0.015f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Appearance")
	FLinearColor ShieldColor = FLinearColor(0.02f, 0.65f, 0.85f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Appearance")
	FText ShieldLabel = NSLOCTEXT("BossUI", "EnergyLinkShield", "실드 활성 · 에너지 링크");

	/** Both damage and healing approach the latest health value at this speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Animation", meta = (ClampMin = "0.1"))
	float InterpSpeed = 5.0f;

private:
	void UpdateShieldStatus(const ABossEnemy* CurrentBoss);

	UPROPERTY(Transient)
	TWeakObjectPtr<ABossEnemy> Boss;

	float DisplayedHealth = 1.0f;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TimerMaterial;
};
