#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Minimap/MinimapMarkerTypes.h"
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
	UMinimapWidget(const FObjectInitializer& ObjectInitializer);

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

	/** 패널 위쪽에 비워 두는 영역 높이(px). 왼쪽 위에 맵 이름을 그린다. 0이면 그리지 않는다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Footer", meta = (ClampMin = "0.0"))
	float HeaderHeight = 64.f;

	/** 패널 아래쪽에 비워 두는 영역 높이(px). 오른쪽에 범례를 그린다. 0이면 그리지 않는다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Footer", meta = (ClampMin = "0.0"))
	float FooterHeight = 66.f;

	/** 맵 이름 글꼴. 비어 있으면 /Game/Fonts/NotoSerifKR_Font */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Footer")
	FSlateFontInfo MapNameFont;

	/** 범례 글꼴 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Footer")
	FSlateFontInfo LegendFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Footer")
	FLinearColor MapNameColor = FLinearColor(0.93f, 0.88f, 0.76f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Footer")
	FLinearColor LegendTextColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.f);

	/** 범례 기호(플레이어·체크포인트) 크기 배율. 지도 위 마커 크기 기준 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Footer", meta = (ClampMin = "0.1"))
	float LegendSymbolScale = 1.5f;

	/** 지도와 하단 영역 사이 구분선 색 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Footer")
	FLinearColor FooterLineColor = FLinearColor(1.f, 1.f, 1.f, 0.18f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Footer")
	FText PlayerLegendText = NSLOCTEXT("Minimap", "LegendPlayer", "플레이어");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Footer")
	FText CheckpointLegendText = NSLOCTEXT("Minimap", "LegendCheckpoint", "체크포인트");

private:
	void DrawObjectMarkers(FSlateWindowElementList& OutDrawElements, int32& LayerId, const FGeometry& Geometry,
		const FVector2D& MapPos, const FVector2D& MapDrawSize, const FVector2D& ViewStart, double Scale, const FLinearColor& WidgetTint) const;

	// 점 하나짜리 마커(도형 또는 아이콘)를 Center에 그린다. 지도 마커와 범례가 같이 쓴다
	void DrawPointMarker(FSlateWindowElementList& OutDrawElements, int32 OutlineLayer, int32 FillLayer, const FGeometry& Geometry,
		const FVector2D& Center, const FMinimapMarkerStyle& Style, UTexture2D* Icon, const FLinearColor& Color, const FLinearColor& WidgetTint) const;

	// 플레이어 표시(검은 테두리의 흰 네모)를 Center에 그린다
	void DrawPlayerMarker(FSlateWindowElementList& OutDrawElements, int32 OutlineLayer, int32 FillLayer, const FGeometry& Geometry,
		const FVector2D& Center, const FLinearColor& WidgetTint, float SizeScale = 1.f) const;

	// 상단 영역: 왼쪽 위 맵 이름과 구분선
	void DrawHeader(FSlateWindowElementList& OutDrawElements, int32& LayerId, const FGeometry& Geometry,
		const FVector2D& HeaderPos, const FVector2D& HeaderSize, const FLinearColor& WidgetTint) const;

	// 하단 영역: 오른쪽 범례(플레이어·체크포인트)
	void DrawFooter(FSlateWindowElementList& OutDrawElements, int32& LayerId, const FGeometry& Geometry,
		const FVector2D& FooterPos, const FVector2D& FooterSize, const FLinearColor& WidgetTint) const;

	TWeakObjectPtr<APawn> Viewer;

	// NativePaint가 const라 매 프레임 UV를 바꾸기 위해 mutable로 둔다
	mutable FSlateBrush MapBrush;
	mutable FSlateBrush IconBrush;
	FSlateColorBrush SolidBrush = FSlateColorBrush(FLinearColor::White);
	FSlateRoundedBoxBrush CircleBrush = FSlateRoundedBoxBrush(FLinearColor::White, 64.f);
};
