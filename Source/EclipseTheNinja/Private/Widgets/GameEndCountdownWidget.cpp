#include "Widgets/GameEndCountdownWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UGameEndCountdownWidget::SetMessage(const FText& InMessage)
{
	if (!this->MessageText)
	{
		BuildLayout();
	}

	this->MessageText->SetText(InMessage);
}

void UGameEndCountdownWidget::SetCount(const int32 InCount)
{
	if (!this->CountText)
	{
		BuildLayout();
	}

	const FText CountDisplay = InCount > 0
		? FText::AsNumber(InCount)
		: FText::GetEmpty();

	this->CountText->SetText(CountDisplay);
}

TSharedRef<SWidget> UGameEndCountdownWidget::RebuildWidget()
{
	if (!this->MessageText)
	{
		BuildLayout();
	}

	return Super::RebuildWidget();
}

void UGameEndCountdownWidget::BuildLayout()
{
	if (!this->WidgetTree)
	{
		this->WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}

	// 화면 전체를 살짝 어둡게 덮고 내용은 중앙에 둔다
	UBorder* Background = this->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
	Background->SetHorizontalAlignment(HAlign_Center);
	Background->SetVerticalAlignment(VAlign_Center);
	this->WidgetTree->RootWidget = Background;

	UVerticalBox* ContentBox = this->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
	Background->SetContent(ContentBox);

	// 안내 메시지
	this->MessageText = this->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MessageText"));
	FSlateFontInfo MessageFont = this->MessageText->GetFont();
	MessageFont.Size = 40;
	this->MessageText->SetFont(MessageFont);
	this->MessageText->SetJustification(ETextJustify::Center);
	this->MessageText->SetShadowOffset(FVector2D(2.0f, 2.0f));
	this->MessageText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));

	UVerticalBoxSlot* MessageSlot = ContentBox->AddChildToVerticalBox(this->MessageText);
	MessageSlot->SetHorizontalAlignment(HAlign_Center);
	MessageSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));

	// 남은 초
	this->CountText = this->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CountText"));
	FSlateFontInfo CountFont = this->CountText->GetFont();
	CountFont.Size = 120;
	this->CountText->SetFont(CountFont);
	this->CountText->SetJustification(ETextJustify::Center);
	this->CountText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.2f, 1.0f)));
	this->CountText->SetShadowOffset(FVector2D(3.0f, 3.0f));
	this->CountText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));

	UVerticalBoxSlot* CountSlot = ContentBox->AddChildToVerticalBox(this->CountText);
	CountSlot->SetHorizontalAlignment(HAlign_Center);
}
