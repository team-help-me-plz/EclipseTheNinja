#include "Widgets/KeyPromptWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

FKeyPromptStyle::FKeyPromptStyle()
{
	Font.Size = 40;
	Font.TypefaceFontName = TEXT("Bold");
}

bool UKeyPromptWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return true;
	}

	// 이미지와 글자를 겹쳐 두고, 둘 중 하나만 보이게 한다(접힌 쪽은 크기에 영향을 주지 않는다)
	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	WidgetTree->RootWidget = RootOverlay;

	// 이미지 모드: 바탕 없이 이미지만 그려 투명 영역이 그대로 비치게 한다
	ImageSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ImageSize"));
	UOverlaySlot* ImageSlot = RootOverlay->AddChildToOverlay(ImageSize);
	ImageSlot->SetHorizontalAlignment(HAlign_Center);
	ImageSlot->SetVerticalAlignment(VAlign_Center);

	KeyImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("KeyImage"));
	ImageSize->AddChild(KeyImage);

	// 글자 모드: 바탕 없이 글자만 그린다
	KeyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("KeyText"));
	KeyText->SetJustification(ETextJustify::Center);
	UOverlaySlot* TextSlot = RootOverlay->AddChildToOverlay(KeyText);
	TextSlot->SetHorizontalAlignment(HAlign_Center);
	TextSlot->SetVerticalAlignment(VAlign_Center);
	DefaultFontObject = KeyText->GetFont().FontObject;

	SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplyStyle(FKeyPromptStyle());
	ShowLabel(FText::GetEmpty());

	return true;
}

void UKeyPromptWidget::ShowImage(UTexture2D* InTexture)
{
	if (!ImageSize || !KeyImage || !KeyText)
	{
		return;
	}

	KeyText->SetVisibility(ESlateVisibility::Collapsed);

	if (!InTexture)
	{
		ImageSize->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	KeyImage->SetBrushFromTexture(InTexture, false);
	UpdateImageSize();

	ImageSize->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UKeyPromptWidget::ShowLabel(const FText& InLabel)
{
	if (!ImageSize || !KeyText)
	{
		return;
	}

	KeyText->SetText(InLabel);

	ImageSize->SetVisibility(ESlateVisibility::Collapsed);
	KeyText->SetVisibility(InLabel.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::HitTestInvisible);
}

void UKeyPromptWidget::ApplyStyle(const FKeyPromptStyle& InStyle, float InRenderScale)
{
	if (!KeyText || !KeyImage)
	{
		return;
	}

	CurrentStyle = InStyle;
	RenderScale = FMath::Max(InRenderScale, 0.1f);

	// 이미지 모드
	KeyImage->SetColorAndOpacity(InStyle.ImageTint);
	UpdateImageSize();

	// 글자 모드: 크기와 외곽선 두께를 렌더 배율만큼 키운다
	FSlateFontInfo Font = InStyle.Font;
	if (!Font.FontObject)
	{
		Font.FontObject = DefaultFontObject;
	}

	Font.Size = FMath::Max(1.0f, Font.Size * RenderScale);
	Font.OutlineSettings.OutlineSize = FMath::RoundToInt(Font.OutlineSettings.OutlineSize * RenderScale);

	KeyText->SetFont(Font);
	KeyText->SetColorAndOpacity(FSlateColor(InStyle.TextColor));
}

void UKeyPromptWidget::UpdateImageSize()
{
	if (!ImageSize || !KeyImage)
	{
		return;
	}

	// 이미지 원본 비율을 지켜 높이에 맞춘다(Space처럼 가로로 긴 키 이미지도 찌그러지지 않게)
	float Aspect = 1.0f;
	const UTexture2D* Texture = Cast<UTexture2D>(KeyImage->GetBrush().GetResourceObject());
	if (Texture && Texture->GetSizeY() > 0)
	{
		Aspect = static_cast<float>(Texture->GetSizeX()) / static_cast<float>(Texture->GetSizeY());
	}

	const float Height = CurrentStyle.ImageHeight * RenderScale;

	ImageSize->SetHeightOverride(Height);
	ImageSize->SetWidthOverride(Height * Aspect);
}
