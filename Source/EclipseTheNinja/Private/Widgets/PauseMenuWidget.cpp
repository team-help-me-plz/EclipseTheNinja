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
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"

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

	UButton* ResumeButton = AddMenuButton(*Menu, LOCTEXT("Resume", "게임 재개"));
	UButton* SettingsButton = AddMenuButton(*Menu, LOCTEXT("Settings", "설정"));
	UButton* MainMenuButton = AddMenuButton(*Menu, LOCTEXT("MainMenu", "메인 화면으로"));
	ResumeButton->OnClicked.AddUniqueDynamic(this, &UPauseMenuWidget::HandleResumeClicked);
	SettingsButton->OnClicked.AddUniqueDynamic(this, &UPauseMenuWidget::HandleSettingsClicked);
	MainMenuButton->OnClicked.AddUniqueDynamic(this, &UPauseMenuWidget::HandleMainMenuClicked);

	// 게임패드 방향키 이동 순서.
	MenuButtons = {ResumeButton, SettingsButton, MainMenuButton};

	return true;
}

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 메뉴를 연 Start 버튼 입력은 추적 전에 지나가므로 컨트롤러가 기억한 장치로 시작한다.
	if (const APlayerCharacterController* Controller = GetOwningPlayer<APlayerCharacterController>())
	{
		FMenuInputFeedback::SetUsingGamepad(Controller->IsUsingGamepad());
	}

	InputFeedback.Start();
}

void UPauseMenuWidget::NativeDestruct()
{
	InputFeedback.Stop();
	Super::NativeDestruct();
}

void UPauseMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 설정 창이 열려 메뉴가 접혀 있으면 색을 빼 둔다.
	TArray<UButton*> Buttons;
	for (UButton* Button : MenuButtons)
	{
		Buttons.Add(Button);
	}

	// 포커스 버튼은 예전 '게임 재개' 강조 모양(남색 채움, 주황 테두리)으로 칠한다.
	const FSlateBrush FocusBrush = EclipseMenuStyle::Box(EclipseMenuStyle::Tile, 3.f, EclipseMenuStyle::Lantern, 1.f);
	InputFeedback.UpdateButtons(this, Buttons, IsVisible(), &FocusBrush);
}

UButton* UPauseMenuWidget::AddMenuButton(UVerticalBox& Menu, const FText& Label)
{
	using namespace EclipseMenuStyle;

	// 모든 버튼을 같은 모양으로 두고, 강조는 포커스 버튼에만 준다.
	UButton* MenuButton = WidgetTree->ConstructWidget<UButton>();
	MenuButton->SetStyle(Button(Ink, Lantern));
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

	const FKey Key = InKeyEvent.GetKey();

	// B는 게임으로 돌아간다.
	if (Key == EKeys::Gamepad_FaceButton_Right)
	{
		if (!InKeyEvent.IsRepeat())
		{
			HandleResumeClicked();
		}
		return FReply::Handled();
	}

	// 방향키패드 위아래로 버튼을 고른다. A를 누르면 포커스된 버튼이 Slate 기본 동작으로 눌린다.
	if (Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Down)
	{
		MoveButtonFocus(Key == EKeys::Gamepad_DPad_Down ? 1 : -1);
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UPauseMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse);
}

FReply UPauseMenuWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	// 마우스로 빈 곳을 누른 경우는 루트에 포커스를 두고, 방향키를 누르면 그때 버튼을 고른다.
	if (InFocusEvent.GetCause() == EFocusCause::Mouse)
	{
		return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
	}

	const int32 Index = MenuButtons.IsValidIndex(PreferredButtonIndex) ? PreferredButtonIndex : 0;

	if (MenuButtons.IsValidIndex(Index) && MenuButtons[Index])
	{
		return FReply::Handled().SetUserFocus(MenuButtons[Index]->TakeWidget(), EFocusCause::Navigation);
	}

	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

FNavigationReply UPauseMenuWidget::NativeOnNavigation(
	const FGeometry& InGeometry,
	const FNavigationEvent& InNavigationEvent,
	const FNavigationReply& InDefaultReply
)
{
	// 빈 목적지를 지정해야 메뉴 안쪽 이동까지 막힌다(Stop은 바깥으로 나가는 것만 막는다).
	if (InNavigationEvent.GetNavigationGenesis() == ENavigationGenesis::Controller)
	{
		return FNavigationReply::Explicit(nullptr);
	}

	return Super::NativeOnNavigation(InGeometry, InNavigationEvent, InDefaultReply);
}

void UPauseMenuWidget::MoveButtonFocus(int32 Direction)
{
	if (MenuButtons.IsEmpty())
	{
		return;
	}

	const int32 Current = MenuButtons.IndexOfByPredicate([this](const UButton* Button)
	{
		return Button && Button->HasUserFocus(GetOwningPlayer());
	});

	// 아직 버튼에 포커스가 없으면 첫 버튼부터 고른다.
	const int32 Next = Current == INDEX_NONE
		? 0
		: (Current + Direction + MenuButtons.Num()) % MenuButtons.Num();
	FocusMenuButton(Next);
}

void UPauseMenuWidget::FocusMenuButton(int32 Index)
{
	if (!MenuButtons.IsValidIndex(Index) || !MenuButtons[Index] || !FSlateApplication::IsInitialized())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	TSharedPtr<FSlateUser> SlateUser = LocalPlayer ? LocalPlayer->GetSlateUser() : nullptr;

	if (!SlateUser)
	{
		return;
	}

	// 같은 위젯에 다시 포커스를 줘도 이전 원인이 남으므로, 먼저 지워 포커스 테두리가 보이게 한다.
	UButton* Button = MenuButtons[Index];
	if (Button->HasUserFocus(GetOwningPlayer()))
	{
		SlateUser->ClearFocus(EFocusCause::SetDirectly);
	}
	SlateUser->SetFocus(Button->TakeWidget(), EFocusCause::Navigation);
	PreferredButtonIndex = Index;
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

	// 설정을 닫고 돌아오면 설정 버튼에 포커스를 둔다.
	PreferredButtonIndex = 1;
	Controller->OpenPauseSettings();
}

void UPauseMenuWidget::HandleMainMenuClicked()
{
	APlayerCharacterController* Controller = GetOwningPlayer<APlayerCharacterController>();
	if (!Controller) return;

	Controller->ReturnToMainMenu();
}

#undef LOCTEXT_NAMESPACE
