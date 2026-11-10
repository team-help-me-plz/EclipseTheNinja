#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "MinimapWidget.generated.h"

/**
 * 미니맵 화면. 위젯 트리 없이 NativePaint에서 미니맵 텍스처와 플레이어 표시를 직접 그린다.
 *  - 맵 전체가 패널에 들어가면 전체를, 너무 크면 플레이어를 중심으로 보이는 부분만 그린다
 *  - 100uu 한 칸의 화면 크기는 MinPixelsPerCell~MaxPixelsPerCell 사이로 맞춘다 (칸 크기가 달라도 같은 월드 크기로 보인다)
 */
UCLASS()
class ECLIPSETHENINJA_API UMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetViewer(APawn* InViewer) { Viewer = InViewer; }

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** 화면 대비 패널 크기 비율 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	FVector2D PanelScreenFraction = FVector2D(0.85, 0.75);

	/** 패널 배경과 맵 사이 여백(px) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	float PanelPadding = 16.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	FLinearColor BackgroundColor = FLinearColor(0.f, 0.f, 0.f, 0.72f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	float MinPixelsPerCell = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	float MaxPixelsPerCell = 14.f;

	/** 플레이어 표시 색. 함정 마커(빨강)와 겹치지 않게 흰색을 쓴다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	FLinearColor MarkerColor = FLinearColor(0.97f, 0.97f, 0.97f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	float MarkerSize = 11.f;

	/** 오브젝트 마커(레버·발판 등)를 그릴지 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Objects")
	bool bShowObjectMarkers = true;

	/** 움직이는 발판 경로 선 굵기(px) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Objects")
	float PathThickness = 2.f;

private:
	void DrawObjectMarkers(FSlateWindowElementList& OutDrawElements, int32& LayerId, const FGeometry& Geometry,
		const FVector2D& MapPos, const FVector2D& MapDrawSize, const FVector2D& ViewStart, double Scale, const FLinearColor& WidgetTint) const;

	TWeakObjectPtr<APawn> Viewer;

	// NativePaint가 const라 매 프레임 UV를 바꾸기 위해 mutable로 둔다
	mutable FSlateBrush MapBrush;
	mutable FSlateBrush IconBrush;
	FSlateColorBrush SolidBrush = FSlateColorBrush(FLinearColor::White);
	FSlateRoundedBoxBrush CircleBrush = FSlateRoundedBoxBrush(FLinearColor::White, 64.f);
};
