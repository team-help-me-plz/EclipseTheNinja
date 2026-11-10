#include "Widgets/PauseMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/PlayerCharacterController.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "EclipseMenuStyle.h"

#define LOCTEXT_NAMESPACE "PauseMenu"

bool UPauseMenuWidget::Initialize()
{
	if (!Super::Initialize()) return false;
	if (!WidgetTree) return false;

	SetIsFocusable(true);

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	using namespace EclipseMenuStyle;

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrush(Box(Ink.CopyWithNewOpacity(0.70f)));
	UOverlaySlot* BackdropSlot = Root->AddChildToOverlay(Backdrop);
	BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
	BackdropSlot->SetVerticalAlignment(VAlign_Fill);

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>();
	PanelSize->SetWidthOverride(674.f);
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(PanelSize);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Center);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrush(Box(Ink, 6.f, Edge, 1.f));
	Panel->SetPadding(FMargin(28.f));
	PanelSize->SetContent(Panel);

	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
	Panel->SetContent(Columns);

	UVerticalBox* Menu = WidgetTree->ConstructWidget<UVerticalBox>();
	UHorizontalBoxSlot* MenuSlot = Columns->AddChildToHorizontalBox(Menu);
	MenuSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	MenuSlot->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));

	Menu->AddChildToVerticalBox(Text(*WidgetTree, LOCTEXT("Brand", "E C L I P S E   /   T H E   N I N J A"), 12, Corona))
		->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	Menu->AddChildToVerticalBox(Text(*WidgetTree, LOCTEXT("Title", "일시정지"), 39))
		->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
	Menu->AddChildToVerticalBox(Rule(*WidgetTree, Edge))
		->SetPadding(FMargin(0.f, 0.f, 0.f, 9.f));

	UBorder* EmblemPanel = WidgetTree->ConstructWidget<UBorder>();
	EmblemPanel->SetBrush(Box(Tile, 4.f, Edge, 1.f));
	EmblemPanel->SetPadding(FMargin(22.f));
	EmblemPanel->SetHorizontalAlignment(HAlign_Center);
	EmblemPanel->SetVerticalAlignment(VAlign_Center);
	Columns->AddChildToHorizontalBox(EmblemPanel)->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Emblem = WidgetTree->ConstructWidget<UVerticalBox>();
	EmblemPanel->SetContent(Emblem);
	Emblem->AddChildToVerticalBox(Eclipse(*WidgetTree, 130.f))->SetHorizontalAlignment(HAlign_Center);
	UVerticalBoxSlot* KeyHint = Emblem->AddChildToVerticalBox(Text(*WidgetTree, LOCTEXT("KeyHint", "ESC   게임 재개"), 13, Mist));
	KeyHint->SetHorizontalAlignment(HAlign_Center);
	KeyHint->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));

	AddMenuButton(*Menu, LOCTEXT("Resume", "게임 재개"), true)->OnClicked.AddUniqueDynamic(this, &UPauseMenuWidget::HandleResumeClicked);
	AddMenuButton(*Menu, LOCTEXT("Settings", "설정"))->OnClicked.AddUniqueDynamic(this, &UPauseMenuWidget::HandleSettingsClicked);
	AddMenuButton(*Menu, LOCTEXT("MainMenu", "메인 화면으로"))->OnClicked.AddUniqueDynamic(this, &UPauseMenuWidget::HandleMainMenuClicked);
	return true;
}

UButton* UPauseMenuWidget::AddMenuButton(UVerticalBox& Menu, const FText& Label, bool bPrimary)
{
	using namespace EclipseMenuStyle;

	UButton* MenuButton = WidgetTree->ConstructWidget<UButton>();
	FButtonStyle Style = Button(Ink, Lantern);
	if (bPrimary)
	{
		Style.SetNormal(Box(Tile, 3.f, Lantern, 1.f));
	}
	MenuButton->SetStyle(Style);
	Menu.AddChildToVerticalBox(MenuButton)->SetPadding(FMargin(0.f, 4.f));

	USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>();
	ButtonSize->SetHeightOverride(44.f);
	UButtonSlot* ContentSlot = CastChecked<UButtonSlot>(MenuButton->AddChild(ButtonSize));
	ContentSlot->SetPadding(FMargin(16.f, 0.f));
	ContentSlot->SetHorizontalAlignment(HAlign_Fill);
	ContentSlot->SetVerticalAlignment(VAlign_Fill);

	UTextBlock* LabelText = Text(*WidgetTree, Label, 17);
	USizeBoxSlot* TextSlot = CastChecked<USizeBoxSlot>(ButtonSize->AddChild(LabelText));
	TextSlot->SetVerticalAlignment(VAlign_Center);
	return MenuButton;
}

FReply UPauseMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const APlayerCharacterController* Controller = Cast<APlayerCharacterController>(GetOwningPlayer());
	if (Controller && Controller->IsPauseMenuKey(InKeyEvent.GetKey()))
	{
		if (!InKeyEvent.IsRepeat())
		{
			HandleResumeClicked();
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UPauseMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse);
}

void UPauseMenuWidget::HandleResumeClicked()
{
	APlayerCharacterController* Controller = GetOwningPlayer<APlayerCharacterController>();
	if (!Controller) return;

	Controller->ResumeFromPauseMenu();
}

void UPauseMenuWidget::HandleSettingsClicked()
{
	APlayerCharacterController* Controller = GetOwningPlayer<APlayerCharacterController>();
	if (!Controller) return;

	Controller->OpenPauseSettings();
}

void UPauseMenuWidget::HandleMainMenuClicked()
{
	APlayerCharacterController* Controller = GetOwningPlayer<APlayerCharacterController>();
	if (!Controller) return;

	Controller->ReturnToMainMenu();
}

#undef LOCTEXT_NAMESPACE
