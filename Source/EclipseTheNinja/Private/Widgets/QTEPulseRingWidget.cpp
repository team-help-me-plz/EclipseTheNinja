#include "Widgets/QTEPulseRingWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"

void UQTEPulseRingWidget::SetSource(UUserWidget* InSourceWidget, const FVector2D& InSourcePivot)
{
    this->SourceWidget = InSourceWidget;
    this->SourcePivot = InSourcePivot;
}

void UQTEPulseRingWidget::Restart()
{
    this->StartTime = FPlatformTime::Seconds();
    Invalidate(EInvalidateWidgetReason::Paint);
}

void UQTEPulseRingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    // 시간에 따라 계속 다시 그려야 하므로 매 프레임 그리기를 무효화한다
    Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UQTEPulseRingWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    // 키 크기를 기준으로 테두리를 시작한다. 아직 배치 전이면 QTE 글리프 기본 크기를 쓴다
    FVector2f KeySize(88.0f, 88.0f);
    if (const UUserWidget* Source = this->SourceWidget.Get())
    {
        const FVector2D Desired = Source->GetDesiredSize();
        if (Desired.X > 1.0 && Desired.Y > 1.0)
        {
            KeySize = FVector2f(Desired);
        }
    }

    // 이 위젯은 피벗 가운데로 키와 같은 지점에 놓인다. 키 컴포넌트의 피벗이 가운데가 아니면 그만큼 키 중심이 옮겨진다
    const FVector2f LocalSize = AllottedGeometry.GetLocalSize();
    const FVector2f KeyCenter = LocalSize * 0.5f + FVector2f(FVector2D(0.5, 0.5) - this->SourcePivot) * KeySize;

    const float Elapsed = static_cast<float>(FPlatformTime::Seconds() - this->StartTime);
    const float SafePeriod = FMath::Max(this->Period, 0.2f);
    const int32 Count = FMath::Clamp(this->RingCount, 1, 4);
    const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;

    for (int32 Index = 0; Index < Count; ++Index)
    {
        // 겹마다 같은 간격으로 늦게 출발한다. 처음 뜰 때 아직 출발하지 않은 겹은 그리지 않는다
        const float RingTime = Elapsed - SafePeriod * Index / Count;
        if (RingTime < 0.0f)
        {
            continue;
        }
        const float Alpha = FMath::Fmod(RingTime, SafePeriod) / SafePeriod;
        const float Grow = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.5f);
        const float Fade = FMath::Pow(1.0f - Alpha, 1.6f);
        const float Thickness = FMath::Lerp(this->StartThickness, this->EndThickness, Alpha);

        // 둥근 상자 테두리는 상자 안쪽으로 그려지므로, 두께만큼 더 키워 키 가장자리 바로 바깥에서 시작해 퍼지게 한다
        const FVector2f RingSize = KeySize + FVector2f(2.0f * (this->Spread * Grow + Thickness + 2.0f));
        const FVector2f RingTopLeft = KeyCenter - RingSize * 0.5f;
        // 키가 길쭉하면 알약 모양, 정사각형이면 원으로 감싼다
        const float Radius = FMath::Min(RingSize.X, RingSize.Y) * 0.5f;

        FLinearColor Color = this->RingColor;
        Color.A *= Fade * Opacity;

        // 둥근 상자 셰이더로 그리면 선을 이어 붙일 때 생기는 이음매·겹침 없이 매끈한 고리가 된다
        // MakeBox의 색은 속 채우기에만 쓰이고 테두리 색은 OutlineSettings가 따로 정하므로, 속은 투명으로 넘겨 키를 가리지 않는다
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
        Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Color), Thickness);
        Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;

        FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
            AllottedGeometry.ToPaintGeometry(RingSize, FSlateLayoutTransform(RingTopLeft)),
            &Brush, ESlateDrawEffect::None, FLinearColor::Transparent);
    }
    return LayerId;
}
