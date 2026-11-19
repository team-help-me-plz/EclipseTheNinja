#include "Widgets/SkipPromptWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void USkipPromptWidget::SetProgress(float InProgress)
{
    const float Clamped = FMath::Clamp(InProgress, 0.0f, 1.0f);
    if (Clamped != this->Progress)
    {
        this->Progress = Clamped;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

TSharedRef<SWidget> USkipPromptWidget::RebuildWidget()
{
    if (this->WidgetTree && !this->WidgetTree->RootWidget)
    {
        USizeBox* Box = this->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SkipPromptBox"));
        Box->SetWidthOverride(this->BoxSize.X);
        Box->SetHeightOverride(this->BoxSize.Y);
        this->WidgetTree->RootWidget = Box;
    }
    return Super::RebuildWidget();
}

int32 USkipPromptWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
    if (Size.X <= 0.0f || Size.Y <= 0.0f)
    {
        return LayerId;
    }

    const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;
    auto Tint = [Opacity](const FLinearColor& Color) { return Color.CopyWithNewOpacity(Color.A * Opacity); };

    // 바탕
    FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(), &this->SolidBrush,
        ESlateDrawEffect::None, Tint(this->BackgroundColor));

    // 선 두께의 절반만큼 안쪽으로 들여 테두리가 상자 밖으로 잘리지 않게 한다.
    // 화면 좌표는 아래로 갈수록 Y가 커지므로 위 가운데 → 오른쪽 위 → 오른쪽 아래 → 왼쪽 아래 → 왼쪽 위 → 위 가운데 순서가 시계방향이다
    const float Inset = this->LineThickness * 0.5f;
    const float Left = Inset;
    const float Top = Inset;
    const float Right = Size.X - Inset;
    const float Bottom = Size.Y - Inset;
    const float CenterX = Size.X * 0.5f;
    const TArray<FVector2f> Corners = {
        FVector2f(CenterX, Top), FVector2f(Right, Top), FVector2f(Right, Bottom),
        FVector2f(Left, Bottom), FVector2f(Left, Top), FVector2f(CenterX, Top)
    };

    // 아직 차지 않은 테두리
    FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(), Corners,
        ESlateDrawEffect::None, Tint(this->TrackColor), true, this->LineThickness);

    // 찬 테두리. 둘레 길이 기준으로 Progress만큼 꼭짓점을 따라가다 마지막 변은 중간에서 끊는다
    if (this->Progress > 0.0f)
    {
        float Perimeter = 0.0f;
        for (int32 Index = 1; Index < Corners.Num(); ++Index)
        {
            Perimeter += FVector2f::Distance(Corners[Index - 1], Corners[Index]);
        }

        float Remaining = Perimeter * this->Progress;
        TArray<FVector2f> Filled = { Corners[0] };
        for (int32 Index = 1; Index < Corners.Num() && Remaining > 0.0f; ++Index)
        {
            const float Length = FVector2f::Distance(Corners[Index - 1], Corners[Index]);
            if (Remaining >= Length)
            {
                Filled.Add(Corners[Index]);
                Remaining -= Length;
            }
            else
            {
                Filled.Add(FMath::Lerp(Corners[Index - 1], Corners[Index], Remaining / Length));
                Remaining = 0.0f;
            }
        }

        if (Filled.Num() >= 2)
        {
            FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(), Filled,
                ESlateDrawEffect::None, Tint(this->FillColor), true, this->LineThickness);
        }
    }

    // 가운데 글자
    if (!this->Label.IsEmpty() && FSlateApplication::IsInitialized())
    {
        const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", this->LabelFontSize);
        const FString Text = this->Label.ToString();
        const FVector2f TextSize = FVector2f(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font));
        const FVector2f Offset = (Size - TextSize) * 0.5f;
        FSlateDrawElement::MakeText(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(Offset)), Text, Font,
            ESlateDrawEffect::None, Tint(this->LabelColor));
    }

    return LayerId;
}
