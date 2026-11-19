#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QTEPulseRingWidget.generated.h"

/**
 * QTE 키 안내 둘레로 파동처럼 퍼지는 테두리를 그리는 위젯.
 *  - 키 위젯(SourceWidget)의 크기에서 시작해 바깥으로 퍼지며 옅어지는 둥근 테두리를 몇 겹 시차를 두고 반복해 그린다
 *  - 키 자체는 건드리지 않는다. 키 안내 위젯 컴포넌트와 같은 위치에 따로 띄운다
 *  - 에셋 없이 NativePaint로 그리므로 블루프린트를 만들지 않아도 된다
 */
UCLASS()
class ECLIPSETHENINJA_API UQTEPulseRingWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 파동이 둘러쌀 키 위젯과 그 위젯 컴포넌트의 피벗. 키가 피벗 중앙이 아니어도 키 가운데에서 퍼지게 한다
    void SetSource(UUserWidget* InSourceWidget, const FVector2D& InSourcePivot);

    // 파동을 처음부터 다시 시작한다
    void Restart();

    // 한 겹이 퍼져 사라지는 데 걸리는 시간
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse Ring", meta = (ClampMin = "0.2", Units = "s"))
    float Period = 1.0f;

    // 동시에 퍼지는 테두리 수. 한 주기 안에서 같은 간격으로 뒤따른다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse Ring", meta = (ClampMin = "1", ClampMax = "4"))
    int32 RingCount = 2;

    // 키 가장자리에서 바깥으로 퍼지는 거리(px)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse Ring", meta = (ClampMin = "0.0"))
    float Spread = 48.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse Ring")
    FLinearColor RingColor = FLinearColor(1.0f, 0.85f, 0.4f, 1.0f);

    // 막 퍼지기 시작할 때와 끝날 때의 테두리 두께(px)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse Ring", meta = (ClampMin = "0.5"))
    float StartThickness = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse Ring", meta = (ClampMin = "0.5"))
    float EndThickness = 1.5f;

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    TWeakObjectPtr<UUserWidget> SourceWidget;
    FVector2D SourcePivot = FVector2D(0.5, 0.5);
    double StartTime = 0.0;
};
