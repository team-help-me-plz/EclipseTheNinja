#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyAwarenessWidget.generated.h"

class UCanvasPanel;
class UEnemyDetectionComponent;
class UImage;
class UProgressBar;
class UTextBlock;
class APlayerController;

/** 모양과 스타일은 WBP가 소유하고, C++은 월드 위치와 감지 상태만 전달한다. */
UCLASS()
class ECLIPSETHENINJA_API UEnemyAwarenessWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeDetection(UEnemyDetectionComponent* InDetection);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // 이 이름과 타입으로 WBP 디자이너의 위젯을 연결한다.
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UCanvasPanel> DiscoveryRange;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UImage> DebugDetectionRange;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UCanvasPanel> GaugeContainer;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UProgressBar> AlertGaugeBar;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UProgressBar> CombatGaugeBar;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UTextBlock> GaugePercentLabel;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UTextBlock> AlertStateLabel;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UTextBlock> CombatStateLabel;

private:
    void UpdateRangeLayout(UWidget* RangeWidget, const FVector& Origin, float Radius, APlayerController* Player);
    void UpdateGauge(const UEnemyDetectionComponent* Component, const FVector& Origin, APlayerController* Player);
    void HidePresentation();

    UPROPERTY(Transient)
    TWeakObjectPtr<UEnemyDetectionComponent> Detection;
};
