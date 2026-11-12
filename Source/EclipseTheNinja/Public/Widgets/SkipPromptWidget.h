#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Brushes/SlateColorBrush.h"
#include "SkipPromptWidget.generated.h"

/**
 * 컷신 건너뛰기 키를 누르고 있는 동안 화면 우측 상단에 뜨는 사각형 안내.
 *  - 위 가운데에서 시작해 시계방향으로, 누른 시간 비율(Progress)만큼 테두리가 찬다
 *  - 위젯 블루프린트 없이 C++만으로 그린다. 모양을 바꾸려면 이 클래스를 상속한 위젯 블루프린트에서 기본값을 고친다
 */
UCLASS()
class ECLIPSETHENINJA_API USkipPromptWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 0~1. 1이면 테두리가 한 바퀴 다 찬다
    UFUNCTION(BlueprintCallable, Category = "Skip Prompt")
    void SetProgress(float InProgress);

    UFUNCTION(BlueprintPure, Category = "Skip Prompt")
    float GetProgress() const { return this->Progress; }

    FVector2D GetBoxSize() const { return this->BoxSize; }

    // 화면 가장자리에서 띄울 거리
    FVector2D GetScreenMargin() const { return this->ScreenMargin; }

protected:
    // 디자이너에서 만든 루트가 없으면 BoxSize 크기의 빈 상자를 루트로 만든다
    virtual TSharedRef<SWidget> RebuildWidget() override;

    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

    // 사각형 크기
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skip Prompt")
    FVector2D BoxSize = FVector2D(72.0f, 72.0f);

    // 화면 우측 상단 모서리에서 띄울 거리
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skip Prompt")
    FVector2D ScreenMargin = FVector2D(48.0f, 48.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skip Prompt", meta = (ClampMin = "0.5"))
    float LineThickness = 3.0f;

    // 사각형 안쪽 바탕
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skip Prompt")
    FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.45f);

    // 아직 차지 않은 테두리
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skip Prompt")
    FLinearColor TrackColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.2f);

    // 찬 테두리
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skip Prompt")
    FLinearColor FillColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.95f);

    // 사각형 가운데 글자
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skip Prompt")
    FText Label = FText::FromString(TEXT("ESC"));

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skip Prompt", meta = (ClampMin = "1"))
    int32 LabelFontSize = 14;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skip Prompt")
    FLinearColor LabelColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

private:
    float Progress = 0.0f;

    // 바탕을 단색으로 칠하는 브러시. 색은 그릴 때 틴트로 준다
    FSlateColorBrush SolidBrush = FSlateColorBrush(FLinearColor::White);
};
