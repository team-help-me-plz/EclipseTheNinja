#include "Widgets/MinimapWidget.h"
#include "Minimap/MinimapSubsystem.h"

#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"
#include "Minimap/MinimapMarkerSettings.h"

namespace
{
	void DrawRect(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry,
		const FVector2D& Position, const FVector2D& Size, const FSlateBrush* Brush, const FLinearColor& Tint)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Position))),
			Brush, ESlateDrawEffect::None, Tint);
	}
}

int32 UMinimapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const UMinimapSubsystem* Minimap = UMinimapSubsystem::Get(this);
	UTexture2D* Texture = Minimap ? Minimap->GetTexture() : nullptr;
	if (!Texture || !Minimap->IsGridReady())
	{
		return LayerId;
	}

	const FLinearColor WidgetTint = InWidgetStyle.GetColorAndOpacityTint();
	const FVector2D LocalSize = FVector2D(AllottedGeometry.GetLocalSize());
	const FVector2D PanelSize = LocalSize * this->PanelScreenFraction;
	const FVector2D PanelPos = (LocalSize - PanelSize) * 0.5;

	// 배경
	DrawRect(OutDrawElements, LayerId, AllottedGeometry, PanelPos, PanelSize, &this->SolidBrush, this->BackgroundColor * WidgetTint);

	// 칸 하나의 화면 크기: 패널에 맞추되 최소·최대 사이로
	const FIntPoint Grid = Minimap->GetGridSize();
	const FVector2D GridSize(Grid.X, Grid.Y);
	const FVector2D InnerSize = PanelSize - FVector2D(this->PanelPadding * 2.f);
	const double Fit = FMath::Min(InnerSize.X / GridSize.X, InnerSize.Y / GridSize.Y);
	// 최소·최대는 100uu 한 칸 기준이다. 칸 크기가 달라도 같은 월드 크기로 보이도록 칸 크기 비율을 곱한다
	const double CellRatio = Minimap->GetCellSize() / 100.0;
	const double Scale = FMath::Clamp(Fit, this->MinPixelsPerCell * CellRatio, this->MaxPixelsPerCell * CellRatio);

	// 보이는 칸 범위. 맵이 패널보다 크면 플레이어를 중심으로 자르고 맵 끝에서 멈춘다
	const FVector2D VisibleCells(FMath::Min(InnerSize.X / Scale, GridSize.X), FMath::Min(InnerSize.Y / Scale, GridSize.Y));
	const APawn* ViewerPawn = this->Viewer.Get();
	const FVector2D PlayerPixel = ViewerPawn ? Minimap->WorldToTexturePixel(ViewerPawn->GetActorLocation()) : GridSize * 0.5;
	const FVector2D ViewStart(
		FMath::Clamp(PlayerPixel.X - VisibleCells.X * 0.5, 0.0, GridSize.X - VisibleCells.X),
		FMath::Clamp(PlayerPixel.Y - VisibleCells.Y * 0.5, 0.0, GridSize.Y - VisibleCells.Y));

	const FVector2D MapDrawSize = VisibleCells * Scale;
	const FVector2D MapPos = PanelPos + (PanelSize - MapDrawSize) * 0.5;

	this->MapBrush.SetResourceObject(Texture);
	this->MapBrush.DrawAs = ESlateBrushDrawType::Image;
	this->MapBrush.ImageSize = MapDrawSize;
	this->MapBrush.SetUVRegion(FBox2f(FVector2f(ViewStart / GridSize), FVector2f((ViewStart + VisibleCells) / GridSize)));
	DrawRect(OutDrawElements, ++LayerId, AllottedGeometry, MapPos, MapDrawSize, &this->MapBrush, WidgetTint);

	// 오브젝트 마커 (레버·발판·함정 등)
	if (this->bShowObjectMarkers)
	{
		DrawObjectMarkers(OutDrawElements, LayerId, AllottedGeometry, MapPos, MapDrawSize, ViewStart, Scale, WidgetTint);
	}

	// 플레이어 위치
	if (ViewerPawn)
	{
		const FVector2D MarkerCenter = MapPos + (PlayerPixel - ViewStart) * Scale;
		const FVector2D Marker(this->MarkerSize);
		DrawRect(OutDrawElements, ++LayerId, AllottedGeometry, MarkerCenter - Marker * 0.5 - FVector2D(2.0), Marker + FVector2D(4.0),
			&this->SolidBrush, FLinearColor::Black * WidgetTint);
		DrawRect(OutDrawElements, ++LayerId, AllottedGeometry, MarkerCenter - Marker * 0.5, Marker, &this->SolidBrush, this->MarkerColor * WidgetTint);
	}

	return LayerId;
}

void UMinimapWidget::DrawObjectMarkers(FSlateWindowElementList& OutDrawElements, int32& LayerId, const FGeometry& Geometry,
	const FVector2D& MapPos, const FVector2D& MapDrawSize, const FVector2D& ViewStart, double Scale, const FLinearColor& WidgetTint) const
{
	const UMinimapSubsystem* Minimap = UMinimapSubsystem::Get(this);
	if (!Minimap)
	{
		return;
	}

	TArray<FMinimapMarkerView> Views;
	Minimap->GetMarkerViews(Views);
	if (Views.IsEmpty())
	{
		return;
	}

	const float DimmedOpacity = GetDefault<UMinimapMarkerSettings>()->DimmedOpacity;
	const FBox2D MapRect(MapPos, MapPos + MapDrawSize);
	const auto ToScreen = [&](const FVector2D& Pixel) { return MapPos + (Pixel - ViewStart) * Scale; };

	// 경로를 먼저 그려 마커 아래에 깔리게 한다
	++LayerId;
	for (const FMinimapMarkerView& View : Views)
	{
		if (View.Path.Num() < 2)
		{
			continue;
		}
		TArray<FVector2f> Points;
		for (const FVector2D& Pixel : View.Path)
		{
			Points.Add(FVector2f(ToScreen(Pixel)));
		}
		// 경로만 그리는 마커(움직이는 발판)는 경로가 유일한 표시라 진하고 굵게, 양 끝·꺾이는 점에 점을 찍는다
		const bool bPathOnly = View.Style->Shape == EMinimapMarkerShape::PathOnly;
		FLinearColor PathColor = View.Color;
		PathColor.A *= bPathOnly ? 0.95f : 0.45f;
		const float Thickness = bPathOnly ? this->PathThickness + 1.5f : this->PathThickness;
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, PathColor * WidgetTint, true, Thickness);
		if (bPathOnly)
		{
			const FVector2D Dot(Thickness + 4.f);
			for (const FVector2f& Point : Points)
			{
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					Geometry.ToPaintGeometry(FVector2f(Dot), FSlateLayoutTransform(Point - FVector2f(Dot) * 0.5f)),
					&this->CircleBrush, ESlateDrawEffect::None, PathColor * WidgetTint);
			}
		}
	}

	// 마커
	const int32 OutlineLayer = ++LayerId;
	const int32 FillLayer = ++LayerId;
	for (const FMinimapMarkerView& View : Views)
	{
		if (View.Style->Shape == EMinimapMarkerShape::PathOnly)
		{
			continue;
		}

		// 땅이 된 오브젝트(착지한 낙하물): 지형과 같은 색으로 채우고 윤곽색 테두리를 두른다
		if (View.bAsGround)
		{
			const FVector2D Min = ToScreen(View.BoundsMin);
			const FVector2D Max = ToScreen(View.BoundsMax);
			const FVector2D Size = (Max - Min).ComponentMax(FVector2D(2.0));
			if (!MapRect.Intersect(FBox2D(Min, Min + Size)))
			{
				continue;
			}
			FSlateDrawElement::MakeBox(OutDrawElements, OutlineLayer,
				Geometry.ToPaintGeometry(FVector2f(Size + FVector2D(2.0)), FSlateLayoutTransform(FVector2f(Min - FVector2D(1.0)))),
				&this->SolidBrush, ESlateDrawEffect::None, FLinearColor(Minimap->GetEdgeColor()) * WidgetTint);
			FSlateDrawElement::MakeBox(OutDrawElements, FillLayer,
				Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Min))),
				&this->SolidBrush, ESlateDrawEffect::None, FLinearColor(Minimap->GetSolidColor()) * WidgetTint);
			continue;
		}

		// 문: 통로를 막는 막대. 닫혀 있으면 채운 막대, 열려 있으면 같은 자리에 테두리만(열린 문틀)
		if (View.Style->Shape == EMinimapMarkerShape::ActorBounds)
		{
			FVector2D Min = ToScreen(View.BoundsMin);
			FVector2D Max = ToScreen(View.BoundsMax);
			// 얇은 문도 보이도록 최소 두께를 둔다
			const FVector2D MinSize(FMath::Max(3.0, (double)View.Style->Size * 0.4));
			const FVector2D Grow = (MinSize - (Max - Min)).ComponentMax(FVector2D::ZeroVector) * 0.5;
			Min -= Grow;
			Max += Grow;
			if (!MapRect.Intersect(FBox2D(Min, Max)))
			{
				continue;
			}
			const FVector2D Size = Max - Min;
			const auto Rect = [&](int32 Layer, const FVector2D& Pos, const FVector2D& RectSize, const FLinearColor& Tint)
			{
				FSlateDrawElement::MakeBox(OutDrawElements, Layer,
					Geometry.ToPaintGeometry(FVector2f(RectSize), FSlateLayoutTransform(FVector2f(Pos))),
					&this->SolidBrush, ESlateDrawEffect::None, Tint);
			};

			if (!View.bStateOn)
			{
				Rect(OutlineLayer, Min - FVector2D(1.5), Size + FVector2D(3.0), FLinearColor::Black * WidgetTint);
				Rect(FillLayer, Min, Size, View.Color * WidgetTint);
			}
			else
			{
				// 열림: 옅게 채우고 테두리를 그린다
				FLinearColor Fill = View.Color;
				Fill.A *= 0.25f;
				Rect(OutlineLayer, Min, Size, Fill * WidgetTint);
				const float Line = 1.5f;
				const FLinearColor Edge = View.Color * WidgetTint;
				Rect(FillLayer, Min, FVector2D(Size.X, Line), Edge);
				Rect(FillLayer, FVector2D(Min.X, Max.Y - Line), FVector2D(Size.X, Line), Edge);
				Rect(FillLayer, Min, FVector2D(Line, Size.Y), Edge);
				Rect(FillLayer, FVector2D(Max.X - Line, Min.Y), FVector2D(Line, Size.Y), Edge);
			}
			continue;
		}

		const FVector2D Center = ToScreen(View.Pixel);
		if (!MapRect.IsInside(Center))
		{
			continue;
		}

		FLinearColor Color = View.Color;
		if (View.bDimmed)
		{
			Color.A *= DimmedOpacity;
		}
		const float Size = View.Style->Size;
		const FVector2D FillSize(Size);
		const FVector2D OutlineSize(Size + 3.f);

		if (View.Icon)
		{
			this->IconBrush.SetResourceObject(View.Icon);
			this->IconBrush.DrawAs = ESlateBrushDrawType::Image;
			this->IconBrush.ImageSize = FillSize;
			FSlateDrawElement::MakeBox(OutDrawElements, FillLayer,
				Geometry.ToPaintGeometry(FVector2f(FillSize), FSlateLayoutTransform(FVector2f(Center - FillSize * 0.5))),
				&this->IconBrush, ESlateDrawEffect::None, Color * WidgetTint);
			continue;
		}

		const FSlateBrush* Brush = View.Style->Shape == EMinimapMarkerShape::Circle ? static_cast<const FSlateBrush*>(&this->CircleBrush) : &this->SolidBrush;
		const bool bDiamond = View.Style->Shape == EMinimapMarkerShape::Diamond;
		const FSlateRenderTransform Rotation = bDiamond ? FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(45.f))) : FSlateRenderTransform();
		// 마름모는 회전하면 커 보이므로 조금 줄인다
		const float ShapeScale = bDiamond ? 0.75f : 1.f;

		const auto DrawShape = [&](int32 Layer, const FVector2D& ShapeSize, const FLinearColor& Tint)
		{
			const FVector2D Scaled = ShapeSize * ShapeScale;
			FSlateDrawElement::MakeBox(OutDrawElements, Layer,
				Geometry.ToPaintGeometry(FVector2f(Scaled), FSlateLayoutTransform(FVector2f(Center - Scaled * 0.5)), Rotation, FVector2f(0.5f, 0.5f)),
				Brush, ESlateDrawEffect::None, Tint);
		};

		FLinearColor OutlineColor = FLinearColor::Black;
		OutlineColor.A = Color.A;
		DrawShape(OutlineLayer, OutlineSize, OutlineColor * WidgetTint);
		DrawShape(FillLayer, FillSize, Color * WidgetTint);
	}
}
