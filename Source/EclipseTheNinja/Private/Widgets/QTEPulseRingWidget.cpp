#include "Widgets/QTEPulseRingWidget.h"
#include "Rendering/DrawElements.h"

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

        // 키 가장자리 바로 바깥에서 시작해 퍼진다
        const FVector2f HalfSize = KeySize * 0.5f + FVector2f(this->Spread * Grow + Thickness * 0.5f + 2.0f);
        // 키가 길쭉하면 알약 모양, 정사각형이면 원으로 감싼다
        const float Radius = FMath::Min(HalfSize.X, HalfSize.Y);
        const FVector2f Straight = HalfSize - FVector2f(Radius);

        // 둥근 사각형 둘레를 선으로만 그린다. 속을 채우지 않아야 키가 가려지지 않는다
        constexpr int32 SegmentsPerCorner = 12;
        TArray<FVector2f> Points;
        Points.Reserve(SegmentsPerCorner * 4 + 5);
        const FVector2f CornerCenters[4] = {
            KeyCenter + FVector2f(Straight.X, Straight.Y), KeyCenter + FVector2f(-Straight.X, Straight.Y),
            KeyCenter + FVector2f(-Straight.X, -Straight.Y), KeyCenter + FVector2f(Straight.X, -Straight.Y)};
        for (int32 Corner = 0; Corner < 4; ++Corner)
        {
            for (int32 Segment = 0; Segment <= SegmentsPerCorner; ++Segment)
            {
                const float Angle = HALF_PI * (Corner + static_cast<float>(Segment) / SegmentsPerCorner);
                const FVector2f Point = CornerCenters[Corner] + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
                // 원 모양이면 호의 끝점과 다음 호의 시작점이 겹친다. 길이 0인 선분은 선 이음을 끊어 동서남북에 틈이 생기므로 뺀다
                if (Points.IsEmpty() || !Points.Last().Equals(Point, 0.01f))
                {
                    Points.Add(Point);
                }
            }
        }
        if (Points.Num() > 2 && Points.Last().Equals(Points[0], 0.01f))
        {
            Points.Pop();
        }
        // 시작점에서도 이음매가 끊기지 않게 처음 두 점까지 다시 이어 겹쳐 닫는다
        // 배열 원소를 참조한 채 같은 배열에 Add하면 TArray 검사에 걸리므로 값으로 복사한다
        const FVector2f FirstPoint = Points[0];
        const FVector2f SecondPoint = Points[1];
        Points.Add(FirstPoint);
        Points.Add(SecondPoint);

        FLinearColor Color = this->RingColor;
        Color.A *= Fade * Opacity;
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(),
            Points, ESlateDrawEffect::None, Color, true, Thickness);
    }
    return LayerId;
}
