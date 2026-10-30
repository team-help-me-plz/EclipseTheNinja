#include "Widgets/BossHealthBarWidget.h"

#include "Character/BossEnemy.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

void UBossHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TimerMaterial = EnergyLinkTimerImage->GetDynamicMaterial();
	if (TimerMaterial)
	{
		TimerMaterial->SetVectorParameterValue(TEXT("OverlayColor"), ShieldColor);
		TimerMaterial->SetScalarParameterValue(TEXT("OverlayOpacity"), 1.0f);
	}
}

void UBossHealthBarWidget::InitBoss(ABossEnemy* InBoss, const FText& InBossName)
{
	Boss = InBoss;
	DisplayedHealth = InBoss ? FMath::Clamp(InBoss->GetHealthRatio(), 0.0f, 1.0f) : 0.0f;
	BossNameText->SetText(InBossName);
	HealthBar->SetPercent(DisplayedHealth);
	ShieldStatusText->SetText(ShieldLabel);
	UpdateShieldStatus(InBoss);
}

void UBossHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const ABossEnemy* CurrentBoss = Boss.Get();
	if (!CurrentBoss)
	{
		RemoveFromParent();
		return;
	}

	const float TargetHealth = CurrentBoss->IsDead() ? 0.0f : FMath::Clamp(CurrentBoss->GetHealthRatio(), 0.0f, 1.0f);
	DisplayedHealth = FMath::FInterpTo(DisplayedHealth, TargetHealth, InDeltaTime, FMath::Max(0.1f, InterpSpeed));
	if (FMath::IsNearlyEqual(DisplayedHealth, TargetHealth, 0.001f))
	{
		DisplayedHealth = TargetHealth;
	}
	HealthBar->SetPercent(DisplayedHealth);

	UpdateShieldStatus(CurrentBoss);

	// Let the final damage animate to zero before dismissing the defeated boss.
	if (CurrentBoss->IsDead() && DisplayedHealth == 0.0f)
	{
		RemoveFromParent();
	}
}

void UBossHealthBarWidget::UpdateShieldStatus(const ABossEnemy* CurrentBoss)
{
	const bool bShielded = CurrentBoss && CurrentBoss->IsEnergyLinkActive() && !CurrentBoss->IsDead();
	HealthBar->SetFillColorAndOpacity(bShielded ? ShieldColor : HealthColor);
	ShieldStatusText->SetVisibility(bShielded ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);

	const float RemainingTime = bShielded ? CurrentBoss->GetEnergyLinkTimeRemaining() : -1.0f;
	const float Duration = bShielded ? CurrentBoss->GetEnergyLinkTimeLimit() : 0.0f;
	const bool bShowTimer = RemainingTime >= 0.0f && Duration > 0.0f;
	const ESlateVisibility TimerVisibility = bShowTimer ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
	EnergyLinkTimerImage->SetVisibility(TimerVisibility);
	EnergyLinkTimerBackground->SetVisibility(TimerVisibility);
	if (TimerMaterial)
	{
		// Reuse the ability cooldown material: Progress 0 is full, 1 is empty.
		const float RemainingRatio = bShowTimer ? FMath::Clamp(RemainingTime / Duration, 0.0f, 1.0f) : 0.0f;
		TimerMaterial->SetScalarParameterValue(TEXT("Progress"), 1.0f - RemainingRatio);
	}
}
