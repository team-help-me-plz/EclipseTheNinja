#include "Widgets/EnemyAwarenessWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Component/EnemyDetectionComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

void UEnemyAwarenessWidget::InitializeDetection(UEnemyDetectionComponent* InDetection)
{
    Detection = InDetection;
}

void UEnemyAwarenessWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // WBP 미리보기에서는 중앙 앵커를 사용한다. 플레이에서는 뷰포트 좌표로 배치한다.
    UWidget* PositionedWidgets[] = { DiscoveryRange.Get(), DebugDetectionRange.Get(), GaugeContainer.Get() };
    for (UWidget* Widget : PositionedWidgets)
    {
        UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(Widget);
        if (!CanvasSlot) continue;

        CanvasSlot->SetAnchors(FAnchors(0.0f));
    }
}

void UEnemyAwarenessWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    const UEnemyDetectionComponent* Component = Detection.Get();
    if (!Component)
    {
        HidePresentation();
        return;
    }

    const AEnemy* Enemy = Cast<AEnemy>(Component->GetOwner());
    if (!Enemy)
    {
        HidePresentation();
        return;
    }

    if (Enemy->IsDead())
    {
        HidePresentation();
        return;
    }

    APlayerController* Player = GetOwningPlayer();
    if (!Player)
    {
        HidePresentation();
        return;
    }

    const FVector Origin = Enemy->GetActorLocation();

    // 작은 발각 원을 표시한다. 큰 감지 원은 디버그 설정에서만 표시한다.
    bool bShowDiscovery = false;
    if (Component->bShowDiscoveryRange || Component->bDrawDebugSight)
    {
        bShowDiscovery = true;
    }

    if (bShowDiscovery)
    {
        UpdateRangeLayout(DiscoveryRange, Origin, Component->GetCurrentDiscoveryRadius(), Player);
    }
    else if (DiscoveryRange)
    {
        DiscoveryRange->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (Component->bDrawDebugSight)
    {
        UpdateRangeLayout(DebugDetectionRange, Origin, Component->GetCurrentDetectionRadius(), Player);
    }
    else if (DebugDetectionRange)
    {
        DebugDetectionRange->SetVisibility(ESlateVisibility::Collapsed);
    }

    UpdateGauge(Component, Origin, Player);
}

void UEnemyAwarenessWidget::UpdateRangeLayout(UWidget* RangeWidget, const FVector& Origin,
    float Radius, APlayerController* Player)
{
    if (!RangeWidget) return;

    UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(RangeWidget);
    if (!CanvasSlot) return;

    // 횡스크롤 평면(XZ)의 원을 화면으로 투영해 WBP 컨테이너의 위치와 크기를 정한다.
    // 모양·색상·테두리는 자식 Image의 브러시를 사용하며 C++에서 덮어쓰지 않는다.
    FVector2D ScreenCenter;
    if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Player, Origin, ScreenCenter, false))
    {
        RangeWidget->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    FVector2D Minimum = ScreenCenter;
    FVector2D Maximum = ScreenCenter;

    // 카메라가 움직여도 원의 화면 경계를 따라가도록 원 둘레의 좌표를 확인한다.
    constexpr int32 SampleCount = 32;
    for (int32 Index = 0; Index < SampleCount; ++Index)
    {
        const float Angle = 2.0f * PI * Index / SampleCount;
        const FVector Offset(FMath::Cos(Angle) * Radius, 0.0f, FMath::Sin(Angle) * Radius);

        FVector2D ScreenPoint;
        if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Player, Origin + Offset, ScreenPoint, false))
        {
            RangeWidget->SetVisibility(ESlateVisibility::Collapsed);
            return;
        }

        Minimum.X = FMath::Min(Minimum.X, ScreenPoint.X);
        Minimum.Y = FMath::Min(Minimum.Y, ScreenPoint.Y);
        Maximum.X = FMath::Max(Maximum.X, ScreenPoint.X);
        Maximum.Y = FMath::Max(Maximum.Y, ScreenPoint.Y);
    }

    CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    CanvasSlot->SetPosition((Minimum + Maximum) * 0.5f);
    CanvasSlot->SetSize(Maximum - Minimum);
    RangeWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UEnemyAwarenessWidget::UpdateGauge(const UEnemyDetectionComponent* Component,
    const FVector& Origin, APlayerController* Player)
{
    if (!GaugeContainer) return;

    bool bShowGauge = false;
    if (Component->bShowAwarenessGauge)
    {
        if (Component->DisplayedGauge > 0.001f || Component->AlertLevel != EEnemyAlertLevel::Normal)
        {
            bShowGauge = true;
        }
    }

    if (!bShowGauge)
    {
        GaugeContainer->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    FVector2D GaugePosition;
    const FVector WorldPosition = Origin + FVector::UpVector * Component->GaugeHeight;
    if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Player, WorldPosition, GaugePosition, false))
    {
        GaugeContainer->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(GaugeContainer);
    if (!CanvasSlot) return;

    // WBP에서 편집한 게이지 크기와 자식 배치는 유지하고 월드 기준 위치만 바꾼다.
    CanvasSlot->SetPosition(GaugePosition);
    GaugeContainer->SetVisibility(ESlateVisibility::HitTestInvisible);

    const float Gauge = FMath::Clamp(Component->DisplayedGauge, 0.0f, 1.0f);
    if (AlertGaugeBar) AlertGaugeBar->SetPercent(Gauge);
    if (CombatGaugeBar) CombatGaugeBar->SetPercent(Gauge);

    // 각 상태의 색상·폰트·문자는 WBP에 있는 개별 위젯에서 수정한다.
    if (AlertGaugeBar) AlertGaugeBar->SetVisibility(ESlateVisibility::Collapsed);
    if (CombatGaugeBar) CombatGaugeBar->SetVisibility(ESlateVisibility::Collapsed);
    if (GaugePercentLabel) GaugePercentLabel->SetVisibility(ESlateVisibility::Collapsed);
    if (AlertStateLabel) AlertStateLabel->SetVisibility(ESlateVisibility::Collapsed);
    if (CombatStateLabel) CombatStateLabel->SetVisibility(ESlateVisibility::Collapsed);

    if (Component->AlertLevel == EEnemyAlertLevel::Combat)
    {
        if (CombatGaugeBar) CombatGaugeBar->SetVisibility(ESlateVisibility::HitTestInvisible);
        if (CombatStateLabel) CombatStateLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
        return;
    }

    if (AlertGaugeBar) AlertGaugeBar->SetVisibility(ESlateVisibility::HitTestInvisible);

    if (Component->AlertLevel == EEnemyAlertLevel::Alert)
    {
        if (AlertStateLabel) AlertStateLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
        return;
    }

    if (!GaugePercentLabel) return;

    GaugePercentLabel->SetText(FText::AsPercent(Gauge));
    GaugePercentLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UEnemyAwarenessWidget::HidePresentation()
{
    if (DiscoveryRange) DiscoveryRange->SetVisibility(ESlateVisibility::Collapsed);
    if (DebugDetectionRange) DebugDetectionRange->SetVisibility(ESlateVisibility::Collapsed);
    if (GaugeContainer) GaugeContainer->SetVisibility(ESlateVisibility::Collapsed);
}
