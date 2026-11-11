#pragma once

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "EclipseUISound.h"

namespace EclipseMenuStyle
{
	inline const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(13, 18, 29));
	inline const FLinearColor Tile = FLinearColor::FromSRGBColor(FColor(36, 44, 68));
	inline const FLinearColor Mist = FLinearColor::FromSRGBColor(FColor(164, 181, 203));
	inline const FLinearColor Lantern = FLinearColor::FromSRGBColor(FColor(196, 150, 102));
	inline const FLinearColor Corona = FLinearColor::FromSRGBColor(FColor(230, 190, 148));
	inline const FLinearColor Paper = FLinearColor::FromSRGBColor(FColor(247, 240, 229));
	inline const FLinearColor Edge = FLinearColor::FromSRGBColor(FColor(68, 81, 108));

	inline FSlateBrush Box(FLinearColor Fill, float Radius = 0.f, FLinearColor Outline = FLinearColor::Transparent, float Width = 0.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius);
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = Width;
		return Brush;
	}

	inline FButtonStyle Button(FLinearColor Fill, FLinearColor Accent)
	{
		FButtonStyle Style;
		Style.SetNormal(Box(Fill, 3.f, Edge, 1.f));
		Style.SetHovered(Box(FMath::Lerp(Fill, Accent, 0.15f), 3.f, Accent, 1.5f));
		Style.SetPressed(Box(Tile, 3.f, Corona, 1.5f));
		Style.SetNormalPadding(FMargin(0.f));
		Style.SetPressedPadding(FMargin(0.f));

		// 스타일을 다시 만들어 덮어써도 호버·선택 사운드가 빠지지 않게 한다
		EclipseUISound::ApplyButtonSounds(Style);
		return Style;
	}

	inline UTextBlock* Text(UWidgetTree& Tree, const FText& Label, int32 Size, FLinearColor Color = Paper, FName Weight = TEXT("Bold"))
	{
		UTextBlock* TextBlock = Tree.ConstructWidget<UTextBlock>();
		TextBlock->SetText(Label);
		TextBlock->SetFont(FCoreStyle::GetDefaultFontStyle(Weight, Size));
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		return TextBlock;
	}

	inline USizeBox* Rule(UWidgetTree& Tree, FLinearColor Color, float Height = 1.f)
	{
		USizeBox* Size = Tree.ConstructWidget<USizeBox>();
		Size->SetHeightOverride(Height);

		UImage* Line = Tree.ConstructWidget<UImage>();
		Line->SetBrush(Box(Color));
		Size->SetContent(Line);
		Size->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Size;
	}

	inline USizeBox* Eclipse(UWidgetTree& Tree, float Diameter)
	{
		USizeBox* Size = Tree.ConstructWidget<USizeBox>();
		Size->SetWidthOverride(Diameter);
		Size->SetHeightOverride(Diameter);
		Size->SetVisibility(ESlateVisibility::HitTestInvisible);

		UOverlay* Rings = Tree.ConstructWidget<UOverlay>();
		Size->SetContent(Rings);

		// 바깥 광륜에서 검은 달 표면까지 겹쳐 일식의 코로나를 그린다.
		const float Insets[] = {0.f, 0.10f, 0.19f};
		const FLinearColor Fills[] = {FLinearColor::Transparent, FLinearColor(0.30f, 0.16f, 0.08f, 0.25f), Ink};
		const FLinearColor Outlines[] = {Edge, Lantern.CopyWithNewOpacity(0.45f), Corona};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Insets); ++Index)
		{
			const float Inset = Diameter * Insets[Index];
			UImage* Ring = Tree.ConstructWidget<UImage>();
			Ring->SetBrush(Box(Fills[Index], Diameter * 0.5f - Inset, Outlines[Index], Index == 2 ? 2.f : 1.f));
			UOverlaySlot* Slot = Rings->AddChildToOverlay(Ring);
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
			Slot->SetPadding(FMargin(Inset));
		}
		return Size;
	}
}
