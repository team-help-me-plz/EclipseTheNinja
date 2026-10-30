

#include "Widgets/MainMenuWidget.h"
#include "Widgets/SettingsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"

namespace MainMenuStyle
{
	// WBP 생성 시 자동으로 들어가는 빈 CanvasPanel은 디자인된 레이아웃으로 보지 않는다
	bool	HasDesignedLayout(const UWidgetTree* InWidgetTree)
	{
		const UPanelWidget*	RootPanel = Cast<UPanelWidget>(InWidgetTree->RootWidget);
		return InWidgetTree->RootWidget && !(RootPanel && RootPanel->GetChildrenCount() == 0);
	}

	FSlateBrush	MakeBoxBrush(const FLinearColor& InColor, const float InRadius = 0.f, const FLinearColor& InOutline = FLinearColor::Transparent, const float InOutlineWidth = 0.f)
	{
		FSlateBrush	Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(InColor);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(InRadius);
		Brush.OutlineSettings.Color = FSlateColor(InOutline);
		Brush.OutlineSettings.Width = InOutlineWidth;
		return Brush;
	}

	// 화면 비율 위치(InAnchor)에 상단 중앙을 맞춰 배치
	void	AddAnchored(UCanvasPanel* InCanvas, UWidget* InWidget, const FVector2D& InAnchor)
	{
		if (UCanvasPanelSlot* CanvasSlot = InCanvas->AddChildToCanvas(InWidget))
		{
			CanvasSlot->SetAnchors(FAnchors(InAnchor.X, InAnchor.Y));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.f));
			CanvasSlot->SetPosition(FVector2D::ZeroVector);
			CanvasSlot->SetAutoSize(true);
		}
	}

	FSlateBrush	MakeRingBrush(const FLinearColor& InFill, const FLinearColor& InOutline, const float InOutlineWidth)
	{
		FSlateBrush	Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(InFill);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Brush.OutlineSettings.Color = FSlateColor(InOutline);
		Brush.OutlineSettings.Width = InOutlineWidth;
		return Brush;
	}
}

bool UMainMenuWidget::Initialize()
{
	const bool	bResult = Super::Initialize();

	// WBP에서 레이아웃을 만들지 않았다면 (루트가 없거나 빈 패널) C++ 기본 레이아웃을 구성
	if (bResult && this->WidgetTree && !MainMenuStyle::HasDesignedLayout(this->WidgetTree))
	{
		this->BuildDefaultLayout();
	}
	// UI 전용 입력 모드에서 포커스를 받을 수 있도록
	SetIsFocusable(true);

	if (this->StartButton)
	{
		this->StartButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::HandleStartClicked);
	}
	if (this->ContinueButton)
	{
		this->ContinueButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::HandleContinueClicked);
	}
	if (this->SettingsButton)
	{
		this->SettingsButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::HandleSettingsClicked);
	}
	if (this->QuitButton)
	{
		this->QuitButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::HandleQuitClicked);
	}
	return bResult;
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (this->BackgroundImage && this->BackgroundTexture)
	{
		this->BackgroundImage->SetBrushFromTexture(this->BackgroundTexture);
		this->BackgroundImage->SetColorAndOpacity(FLinearColor::White);
	}
	if (this->BackgroundDimImage)
	{
		const float	DimOpacity = this->BackgroundTexture ? this->BackgroundDimOpacity : 0.f;
		this->BackgroundDimImage->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, DimOpacity));
	}
	if (this->TitleContainer)
	{
		this->TitleContainer->SetVisibility(this->bShowTitle ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (this->TitleTextBlock)
	{
		this->TitleTextBlock->SetText(this->TitleText);
	}
	if (this->SubtitleTextBlock)
	{
		this->SubtitleTextBlock->SetText(this->SubtitleText);
	}
	if (this->StartButton)
	{
		this->StartButton->SetKeyboardFocus();
	}
}

void UMainMenuWidget::BuildDefaultLayout()
{
	UOverlay*	Root = this->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	this->WidgetTree->RootWidget = Root;

	// 배경
	this->BackgroundImage = this->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackgroundImage"));
	this->BackgroundImage->SetBrush(MainMenuStyle::MakeBoxBrush(FLinearColor::White));
	this->BackgroundImage->SetColorAndOpacity(this->BackgroundColor);
	if (UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(this->BackgroundImage))
	{
		BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
		BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// 배경 텍스처 위 어두운 막 (버튼 가독성용)
	this->BackgroundDimImage = this->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackgroundDimImage"));
	this->BackgroundDimImage->SetBrush(MainMenuStyle::MakeBoxBrush(FLinearColor::White));
	if (UOverlaySlot* DimSlot = Root->AddChildToOverlay(this->BackgroundDimImage))
	{
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// 일식 링 (키 아트처럼 우측 상단, 배경 텍스처가 없을 때만)
	if (!this->BackgroundTexture)
	{
		const FLinearColor	CoronaColor(1.f, 0.55f, 0.25f, 1.f);
		const float			RingSizes[] = { 330.f, 300.f };
		const FLinearColor	RingOutlines[] = { FLinearColor(CoronaColor.R, CoronaColor.G, CoronaColor.B, 0.15f), CoronaColor };
		const float			RingWidths[] = { 18.f, 2.f };

		for (int32 i = 0; i < UE_ARRAY_COUNT(RingSizes); ++i)
		{
			USizeBox*	RingBox = this->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			RingBox->SetWidthOverride(RingSizes[i]);
			RingBox->SetHeightOverride(RingSizes[i]);

			UImage*		Ring = this->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Ring->SetBrush(MainMenuStyle::MakeRingBrush(FLinearColor::Black, RingOutlines[i], RingWidths[i]));
			RingBox->AddChild(Ring);

			const float	Offset = (RingSizes[0] - RingSizes[i]) * 0.5f;
			if (UOverlaySlot* RingSlot = Root->AddChildToOverlay(RingBox))
			{
				RingSlot->SetHorizontalAlignment(HAlign_Right);
				RingSlot->SetVerticalAlignment(VAlign_Top);
				RingSlot->SetPadding(FMargin(0.f, 70.f + Offset, 180.f + Offset, 0.f));
			}
		}
	}

	// 타이틀 / 버튼은 화면 비율 기준 앵커로 배치 (늘어나는 배경 이미지와 함께 움직이도록)
	UCanvasPanel*	Canvas = this->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("LayoutCanvas"));
	if (UOverlaySlot* CanvasSlot = Root->AddChildToOverlay(Canvas))
	{
		CanvasSlot->SetHorizontalAlignment(HAlign_Fill);
		CanvasSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox*	TitleBox = this->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleBox"));
	MainMenuStyle::AddAnchored(Canvas, TitleBox, this->TitleAnchor);
	this->TitleContainer = TitleBox;

	UVerticalBox*	MenuBox = this->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
	MainMenuStyle::AddAnchored(Canvas, MenuBox, this->MenuAnchor);

	this->TitleTextBlock = this->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleTextBlock"));
	this->TitleTextBlock->SetText(this->TitleText);
	this->TitleTextBlock->SetFont(this->TitleFont);
	this->TitleTextBlock->SetColorAndOpacity(FSlateColor(this->TitleColor));
	this->TitleTextBlock->SetShadowOffset(FVector2D(0.f, 4.f));
	this->TitleTextBlock->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	this->TitleTextBlock->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* TitleSlot = TitleBox->AddChildToVerticalBox(this->TitleTextBlock))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// ── THE NINJA ── 형태의 부제
	UHorizontalBox*	SubtitleRow = this->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SubtitleRow"));
	if (UVerticalBoxSlot* SubtitleSlot = TitleBox->AddChildToVerticalBox(SubtitleRow))
	{
		SubtitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	FSlateFontInfo	SubtitleFontInfo = this->SubtitleFont;
	if (SubtitleFontInfo.LetterSpacing == 0)
	{
		SubtitleFontInfo.LetterSpacing = 400;
	}
	this->SubtitleTextBlock = this->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SubtitleTextBlock"));
	this->SubtitleTextBlock->SetText(this->SubtitleText);
	this->SubtitleTextBlock->SetFont(SubtitleFontInfo);
	this->SubtitleTextBlock->SetColorAndOpacity(FSlateColor(this->TitleColor));

	for (int32 i = 0; i < 3; ++i)
	{
		UWidget*	Child = this->SubtitleTextBlock;
		FMargin		SlotPadding(0.f);

		if (i != 1)
		{
			USizeBox*	LineBox = this->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			LineBox->SetWidthOverride(150.f);
			LineBox->SetHeightOverride(2.f);

			UImage*		Line = this->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Line->SetBrush(MainMenuStyle::MakeBoxBrush(FLinearColor(this->TitleColor.R, this->TitleColor.G, this->TitleColor.B, 0.6f)));
			LineBox->AddChild(Line);

			Child = LineBox;
			SlotPadding = (i == 0) ? FMargin(0.f, 0.f, 20.f, 0.f) : FMargin(20.f, 0.f, 0.f, 0.f);
		}
		if (UHorizontalBoxSlot* ChildSlot = SubtitleRow->AddChildToHorizontalBox(Child))
		{
			ChildSlot->SetVerticalAlignment(VAlign_Center);
			ChildSlot->SetPadding(SlotPadding);
		}
	}

	this->StartButton = this->MakeMenuButton(MenuBox, FText::FromString(TEXT("게임 시작")));
	this->ContinueButton = this->MakeMenuButton(MenuBox, FText::FromString(TEXT("이어하기")));
	this->SettingsButton = this->MakeMenuButton(MenuBox, FText::FromString(TEXT("설정")));
	this->QuitButton = this->MakeMenuButton(MenuBox, FText::FromString(TEXT("게임 종료")));
}

UButton* UMainMenuWidget::MakeMenuButton(UVerticalBox* InParent, const FText& InLabel)
{
	USizeBox*	ButtonBox = this->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ButtonBox->SetWidthOverride(320.f);
	ButtonBox->SetHeightOverride(56.f);

	FButtonStyle	Style;
	Style.SetNormal(MainMenuStyle::MakeBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.04f), 4.f, FLinearColor(1.f, 1.f, 1.f, 0.15f), 1.f));
	Style.SetHovered(MainMenuStyle::MakeBoxBrush(FLinearColor(this->AccentColor.R, this->AccentColor.G, this->AccentColor.B, 0.25f), 4.f, this->AccentColor, 1.f));
	Style.SetPressed(MainMenuStyle::MakeBoxBrush(FLinearColor(this->AccentColor.R, this->AccentColor.G, this->AccentColor.B, 0.5f), 4.f, this->AccentColor, 1.f));
	Style.SetNormalPadding(FMargin(0.f));
	Style.SetPressedPadding(FMargin(0.f));

	UButton*	Button = this->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetStyle(Style);
	ButtonBox->AddChild(Button);

	UTextBlock*	Label = this->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Label->SetText(InLabel);
	Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(Button->AddChild(Label)))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (UVerticalBoxSlot* ButtonSlot = InParent->AddChildToVerticalBox(ButtonBox))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		ButtonSlot->SetPadding(FMargin(0.f, 6.f));
	}
	return Button;
}

void UMainMenuWidget::HandleStartClicked()
{
	if (this->StartLevel.IsNull())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[UMainMenuWidget::HandleStartClicked] %s의 StartLevel이 지정되지 않았습니다"),
			*GetName()
		)
		return;
	}
	// UI 전용 입력 모드가 다음 레벨까지 남지 않도록 복구.
	// 입력 모드는 레벨이 바뀌어도 뷰포트에 남는다. GameOnly는 마우스를 숨기고 가운데에 묶어
	// 커서 위치로 조준·회전하는 게임 입력이 깨지므로, 커서를 보이게 둔 GameAndUI로 넘긴다
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeGameAndUI	InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetShowMouseCursor(true);
	}
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, this->StartLevel);
}

void UMainMenuWidget::HandleContinueClicked()
{
	// TODO: 세이브 시스템 구현 후 이어하기 기능 연결
}

void UMainMenuWidget::HandleSettingsClicked()
{
	if (!this->SettingsWidget)
	{
		const TSubclassOf<USettingsWidget>	WidgetClass = this->SettingsWidgetClass ? this->SettingsWidgetClass : TSubclassOf<USettingsWidget>(USettingsWidget::StaticClass());

		this->SettingsWidget = CreateWidget<USettingsWidget>(GetOwningPlayer(), WidgetClass);
		if (!this->SettingsWidget)
		{
			return;
		}
		this->SettingsWidget->OnSettingsClosed.AddUniqueDynamic(this, &UMainMenuWidget::HandleSettingsClosed);
	}
	if (!this->SettingsWidget->IsInViewport())
	{
		this->SettingsWidget->AddToViewport(10);
	}
	this->SettingsWidget->SetKeyboardFocus();
}

void UMainMenuWidget::HandleSettingsClosed()
{
	if (this->SettingsButton)
	{
		this->SettingsButton->SetKeyboardFocus();
	}
}

void UMainMenuWidget::HandleQuitClicked()
{
	// PIE에서는 플레이 세션 종료, 빌드에서는 프로그램 종료
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
