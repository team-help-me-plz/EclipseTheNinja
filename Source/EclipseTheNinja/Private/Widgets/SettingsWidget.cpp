

#include "Widgets/SettingsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/GameUserSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/InputSettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "Widgets/KeyBindingSelector.h"
#include "InputMappingContext.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Subsystem/UserAudioSettingsSubsystem.h"
#include "EclipseMenuStyle.h"

namespace SettingsStyle
{
	const TArray<FString>	WindowModeOptions = { TEXT("전체 화면"), TEXT("테두리 없는 창"), TEXT("창 모드") };
	const TArray<FString>	QualityOptions = { TEXT("낮음"), TEXT("중간"), TEXT("높음"), TEXT("에픽"), TEXT("시네마틱") };
	const TArray<FString>	FrameLimitOptions = { TEXT("30"), TEXT("60"), TEXT("120"), TEXT("144"), TEXT("무제한") };
	const TArray<float>		FrameLimitValues = { 30.f, 60.f, 120.f, 144.f, 0.f };

	EWindowMode::Type	ToWindowMode(const int32 InIndex)
	{
		switch (InIndex)
		{
		case 0:		return EWindowMode::Fullscreen;
		case 1:		return EWindowMode::WindowedFullscreen;
		default:	return EWindowMode::Windowed;
		}
	}

	int32	ToWindowModeIndex(const EWindowMode::Type InMode)
	{
		switch (InMode)
		{
		case EWindowMode::Fullscreen:			return 0;
		case EWindowMode::WindowedFullscreen:	return 1;
		default:								return 2;
		}
	}

	FString	ToResolutionString(const FIntPoint& InResolution)
	{
		return FString::Printf(TEXT("%d x %d"), InResolution.X, InResolution.Y);
	}
}

bool USettingsWidget::Initialize()
{
	const bool	bResult = Super::Initialize();

	// WBP에서 레이아웃을 만들지 않았다면 (루트가 없거나 빈 패널) C++ 기본 레이아웃을 구성
	const UPanelWidget*	RootPanel = this->WidgetTree ? Cast<UPanelWidget>(this->WidgetTree->RootWidget) : nullptr;
	if (bResult && this->WidgetTree && (!this->WidgetTree->RootWidget || (RootPanel && RootPanel->GetChildrenCount() == 0)))
	{
		this->BuildDefaultLayout();
		SetIsFocusable(true);
	}
	return bResult;
}

void USettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	this->LoadCurrentSettings();
	this->RefreshKeyBindingList();
	this->SelectTab(ESettingsTab::Display);
}

FReply USettingsWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		for (const UKeyBindingSelector* Selector : KeyBindingSelectors)
		{
			if (Selector->GetIsSelectingKey()) return FReply::Unhandled();
		}
		if (InKeyEvent.IsRepeat()) return FReply::Handled();

		// 목록을 닫고 설정으로 포커스를 돌려 PIE 단축키로 빠지지 않게 한다.
		const UComboBoxString* ComboBoxes[] = {WindowModeComboBox, ResolutionComboBox, QualityComboBox, FrameLimitComboBox};
		for (const UComboBoxString* ComboBox : ComboBoxes)
		{
			if (ComboBox && ComboBox->IsOpen())
			{
				FSlateApplication::Get().DismissAllMenus();
				return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::SetDirectly);
			}
		}

		CloseSettings();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void USettingsWidget::BuildDefaultLayout()
{
	UOverlay*	Root = this->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	this->WidgetTree->RootWidget = Root;

	// 뒤쪽 메인 화면을 어둡게 가리는 막
	UImage*		Dim = this->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DimImage"));
	Dim->SetBrush(EclipseMenuStyle::Box(EclipseMenuStyle::Ink.CopyWithNewOpacity(0.76f)));
	if (UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim))
	{
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);
	}

	USizeBox*	PanelBox = this->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelBox"));
	PanelBox->SetWidthOverride(900.f);
	PanelBox->SetHeightOverride(520.f);
	if (UOverlaySlot* PanelSlot = Root->AddChildToOverlay(PanelBox))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
	}

	UBorder*	Panel = this->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrush(EclipseMenuStyle::Box(EclipseMenuStyle::Ink, 6.f, EclipseMenuStyle::Edge, 1.f));
	Panel->SetPadding(FMargin(32.f, 24.f));
	PanelBox->AddChild(Panel);

	UVerticalBox*	PanelContent = this->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PanelContent"));
	Panel->SetContent(PanelContent);

	// 상단 제목과 일식 문양
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	PanelContent->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
	UVerticalBox* Heading = WidgetTree->ConstructWidget<UVerticalBox>();
	Header->AddChildToHorizontalBox(Heading)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Heading->AddChildToVerticalBox(MakeText(FText::FromString(TEXT("E C L I P S E   /   T H E   N I N J A")), 12, AccentColor))
		->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	Heading->AddChildToVerticalBox(MakeText(FText::FromString(TEXT("환경설정")), 37));
	Header->AddChildToHorizontalBox(EclipseMenuStyle::Eclipse(*WidgetTree, 62.f))->SetVerticalAlignment(VAlign_Center);
	PanelContent->AddChildToVerticalBox(EclipseMenuStyle::Rule(*WidgetTree, EclipseMenuStyle::Edge))
		->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

	// 왼쪽에서 분류를 고르고 오른쪽에서 값을 조정한다.
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	PanelContent->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	USizeBox* NavigationBox = WidgetTree->ConstructWidget<USizeBox>();
	NavigationBox->SetWidthOverride(175.f);
	Body->AddChildToHorizontalBox(NavigationBox)->SetPadding(FMargin(0.f, 0.f, 22.f, 0.f));

	UVerticalBox* TabBar = WidgetTree->ConstructWidget<UVerticalBox>();
	NavigationBox->SetContent(TabBar);

	const FText	TabLabels[] = {
		FText::FromString(TEXT("01   디스플레이")),
		FText::FromString(TEXT("02   오디오")),
		FText::FromString(TEXT("03   그래픽")),
		FText::FromString(TEXT("04   조작키")),
	};
	for (const FText& TabLabel : TabLabels)
	{
		UButton*	TabButton = this->MakeTextButton(TabLabel, 0.f);
		this->TabButtons.Add(TabButton);
		TabBar->AddChildToVerticalBox(TabButton)->SetPadding(FMargin(0.f, 0.f, 0.f, 7.f));
	}
	this->TabButtons[static_cast<int32>(ESettingsTab::Display)]->OnClicked.AddUniqueDynamic(this, &USettingsWidget::HandleDisplayTabClicked);
	this->TabButtons[static_cast<int32>(ESettingsTab::Audio)]->OnClicked.AddUniqueDynamic(this, &USettingsWidget::HandleAudioTabClicked);
	this->TabButtons[static_cast<int32>(ESettingsTab::Graphics)]->OnClicked.AddUniqueDynamic(this, &USettingsWidget::HandleGraphicsTabClicked);
	this->TabButtons[static_cast<int32>(ESettingsTab::Controls)]->OnClicked.AddUniqueDynamic(this, &USettingsWidget::HandleControlsTabClicked);

	// 탭 페이지 (ESettingsTab 순서와 동일하게 추가)
	this->TabSwitcher = this->WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("TabSwitcher"));
	if (UHorizontalBoxSlot* SwitcherSlot = Body->AddChildToHorizontalBox(this->TabSwitcher))
	{
		SwitcherSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// 디스플레이
	{
		const FText DisplayHint = IsPlayingInEditor()
			? FText::FromString(TEXT("화면 모드와 해상도는 독립 실행형 게임에서 변경할 수 있습니다."))
			: FText::FromString(TEXT("화면 표시 방식과 해상도를 설정합니다"));
		UVerticalBox* Page = MakePage(DisplayHint);

		this->WindowModeComboBox = MakeSettingsComboBox();
		for (const FString& Option : SettingsStyle::WindowModeOptions)
		{
			this->WindowModeComboBox->AddOption(Option);
		}
		WindowModeComboBox->OnSelectionChanged.AddUniqueDynamic(this, &USettingsWidget::HandleWindowModeChanged);
		this->AddRow(Page, FText::FromString(TEXT("화면 모드")), this->WindowModeComboBox);

		this->ResolutionComboBox = MakeSettingsComboBox();
		ResolutionComboBox->OnSelectionChanged.AddUniqueDynamic(this, &USettingsWidget::HandleResolutionChanged);
		this->AddRow(Page, FText::FromString(TEXT("해상도")), this->ResolutionComboBox);

		this->VSyncCheckBox = this->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
		VSyncCheckBox->OnCheckStateChanged.AddUniqueDynamic(this, &USettingsWidget::HandleVSyncChanged);
		this->AddRow(Page, FText::FromString(TEXT("수직 동기화")), this->VSyncCheckBox);
	}

	// 오디오
	{
		UVerticalBox*	Page = this->MakePage(FText::FromString(TEXT("음량을 조절합니다")));

		this->MasterVolumeSlider = this->AddVolumeRow(Page, FText::FromString(TEXT("전체 음량")), this->MasterVolumeText);
		this->MusicVolumeSlider = this->AddVolumeRow(Page, FText::FromString(TEXT("배경음")), this->MusicVolumeText);
		this->SFXVolumeSlider = this->AddVolumeRow(Page, FText::FromString(TEXT("효과음")), this->SFXVolumeText);

		this->MasterVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &USettingsWidget::HandleMasterVolumeChanged);
		this->MusicVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &USettingsWidget::HandleMusicVolumeChanged);
		this->SFXVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &USettingsWidget::HandleSFXVolumeChanged);
	}

	// 그래픽
	{
		UVerticalBox*	Page = this->MakePage(FText::FromString(TEXT("그래픽 품질과 프레임을 설정합니다")));

		this->QualityComboBox = MakeSettingsComboBox();
		for (const FString& Option : SettingsStyle::QualityOptions)
		{
			this->QualityComboBox->AddOption(Option);
		}
		QualityComboBox->OnSelectionChanged.AddUniqueDynamic(this, &USettingsWidget::HandleQualityChanged);
		this->AddRow(Page, FText::FromString(TEXT("전체 품질")), this->QualityComboBox);

		this->FrameLimitComboBox = MakeSettingsComboBox();
		for (const FString& Option : SettingsStyle::FrameLimitOptions)
		{
			this->FrameLimitComboBox->AddOption(Option);
		}
		FrameLimitComboBox->OnSelectionChanged.AddUniqueDynamic(this, &USettingsWidget::HandleFrameLimitChanged);
		this->AddRow(Page, FText::FromString(TEXT("프레임 제한")), this->FrameLimitComboBox);
	}

	// 조작키
	{
		UVerticalBox*	Page = this->MakePage(FText::FromString(TEXT("키를 클릭한 뒤 새 키를 누르세요. ESC로 변경을 취소합니다.")));

		UScrollBox*	Scroll = this->WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		if (UVerticalBoxSlot* ScrollSlot = Page->AddChildToVerticalBox(Scroll))
		{
			ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		this->KeyBindingList = this->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("KeyBindingList"));
		Scroll->AddChild(this->KeyBindingList);

		UHorizontalBox* ControlsFooter = WidgetTree->ConstructWidget<UHorizontalBox>();
		Page->AddChildToVerticalBox(ControlsFooter)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

		KeyBindingStatus = MakeText(FText::GetEmpty(), 13, EclipseMenuStyle::Mist);
		KeyBindingStatus->SetAutoWrapText(true);
		UHorizontalBoxSlot* StatusSlot = ControlsFooter->AddChildToHorizontalBox(KeyBindingStatus);
		StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		StatusSlot->SetVerticalAlignment(VAlign_Center);
		StatusSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

		UButton* ResetButton = MakeTextButton(FText::FromString(TEXT("기본값 복원")), 155.f);
		ResetButton->OnClicked.AddUniqueDynamic(this, &USettingsWidget::ResetKeyBindings);
		ControlsFooter->AddChildToHorizontalBox(ResetButton);
	}

	PanelContent->AddChildToVerticalBox(EclipseMenuStyle::Rule(*WidgetTree, EclipseMenuStyle::Edge))
		->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));

	// 하단 버튼
	UHorizontalBox*	Footer = this->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Footer"));
	if (UVerticalBoxSlot* FooterSlot = PanelContent->AddChildToVerticalBox(Footer))
	{
		FooterSlot->SetHorizontalAlignment(HAlign_Right);
		FooterSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
	}

	UTextBlock* SaveHint = MakeText(FText::FromString(TEXT("변경 사항은 즉시 적용되고 자동으로 저장됩니다.")), 14, EclipseMenuStyle::Mist);
	if (UHorizontalBoxSlot* HintSlot = Footer->AddChildToHorizontalBox(SaveHint))
	{
		HintSlot->SetVerticalAlignment(VAlign_Center);
		HintSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
	}

	UButton*	CloseButton = this->MakeTextButton(FText::FromString(TEXT("ESC   닫기")), 148.f);
	CloseButton->OnClicked.AddUniqueDynamic(this, &USettingsWidget::HandleCloseClicked);
	Footer->AddChildToHorizontalBox(CloseButton);
}

UVerticalBox* USettingsWidget::MakePage(const FText& InHeader)
{
	UVerticalBox*	Page = this->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	this->TabSwitcher->AddChild(Page);

	if (UVerticalBoxSlot* HeaderSlot = Page->AddChildToVerticalBox(this->MakeText(InHeader, 14, EclipseMenuStyle::Mist)))
	{
		HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}
	return Page;
}

void USettingsWidget::AddRow(UVerticalBox* InPage, const FText& InLabel, UWidget* InValue)
{
	UBorder*	RowBorder = this->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	RowBorder->SetBrush(EclipseMenuStyle::Box(EclipseMenuStyle::Tile.CopyWithNewOpacity(0.65f), 4.f));
	RowBorder->SetPadding(FMargin(16.f, 8.f));

	UHorizontalBox*	Row = this->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RowBorder->SetContent(Row);

	if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(this->MakeText(InLabel, 16)))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	USizeBox*	ValueBox = this->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ValueBox->SetWidthOverride(248.f);
	ValueBox->AddChild(InValue);
	if (UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(ValueBox))
	{
		ValueSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (UVerticalBoxSlot* RowSlot = InPage->AddChildToVerticalBox(RowBorder))
	{
		RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}
}

USlider* USettingsWidget::AddVolumeRow(UVerticalBox* InPage, const FText& InLabel, TObjectPtr<UTextBlock>& OutValueText)
{
	UHorizontalBox*	ValueRow = this->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	USlider*	Slider = this->WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	Slider->SetMinValue(0.f);
	Slider->SetMaxValue(1.f);
	Slider->SetStepSize(0.01f);
	Slider->SetSliderBarColor(EclipseMenuStyle::Mist.CopyWithNewOpacity(0.6f));
	Slider->SetSliderHandleColor(this->AccentColor);
	if (UHorizontalBoxSlot* SliderSlot = ValueRow->AddChildToHorizontalBox(Slider))
	{
		SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SliderSlot->SetVerticalAlignment(VAlign_Center);
	}

	USizeBox*	TextBox = this->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	TextBox->SetWidthOverride(64.f);
	OutValueText = this->MakeText(FText::GetEmpty(), 16, EclipseMenuStyle::Mist);
	OutValueText->SetJustification(ETextJustify::Right);
	TextBox->AddChild(OutValueText);
	if (UHorizontalBoxSlot* TextSlot = ValueRow->AddChildToHorizontalBox(TextBox))
	{
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}

	this->AddRow(InPage, InLabel, ValueRow);
	return Slider;
}

UComboBoxString* USettingsWidget::MakeSettingsComboBox()
{
	UComboBoxString* ComboBox = WidgetTree->ConstructWidget<UComboBoxString>();
	FComboBoxStyle Style = ComboBox->GetWidgetStyle();
	Style.ComboButtonStyle.SetButtonStyle(EclipseMenuStyle::Button(EclipseMenuStyle::Ink, AccentColor));
	Style.ComboButtonStyle.SetMenuBorderBrush(EclipseMenuStyle::Box(EclipseMenuStyle::Ink, 3.f, EclipseMenuStyle::Edge, 1.f));
	Style.ComboButtonStyle.DownArrowImage.TintColor = FSlateColor(AccentColor);
	ComboBox->SetWidgetStyle(Style);
	FTableRowStyle ItemStyle = ComboBox->GetItemStyle();
	const FSlateBrush Highlight = EclipseMenuStyle::Box(EclipseMenuStyle::Tile, 2.f, AccentColor, 1.f);
	ItemStyle.SetActiveBrush(Highlight);
	ItemStyle.SetActiveHoveredBrush(Highlight);
	ItemStyle.SetInactiveBrush(Highlight);
	ItemStyle.SetInactiveHoveredBrush(Highlight);
	ItemStyle.SetEvenRowBackgroundHoveredBrush(Highlight);
	ItemStyle.SetOddRowBackgroundHoveredBrush(Highlight);
	ComboBox->SetItemStyle(ItemStyle);
	ComboBox->OnGenerateWidgetEvent.BindDynamic(this, &USettingsWidget::MakeComboOption);

	return ComboBox;
}

UWidget* USettingsWidget::MakeComboOption(FString Option)
{
	return MakeText(FText::FromString(Option), 15, EclipseMenuStyle::Paper);
}

UButton* USettingsWidget::MakeTextButton(const FText& InLabel, const float InWidth)
{
	UButton*	Button = this->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetStyle(EclipseMenuStyle::Button(EclipseMenuStyle::Tile, this->AccentColor));

	USizeBox*	LabelBox = this->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelBox->SetHeightOverride(44.f);
	if (InWidth > 0.f)
	{
		LabelBox->SetWidthOverride(InWidth);
	}
	UTextBlock*	Label = this->MakeText(InLabel, 18);
	Label->SetJustification(ETextJustify::Center);
	if (USizeBoxSlot* LabelBoxSlot = Cast<USizeBoxSlot>(LabelBox->AddChild(Label)))
	{
		LabelBoxSlot->SetHorizontalAlignment(HAlign_Fill);
		LabelBoxSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(Button->AddChild(LabelBox)))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Fill);
		LabelSlot->SetVerticalAlignment(VAlign_Fill);
	}
	return Button;
}

UTextBlock* USettingsWidget::MakeText(const FText& InText, const int32 InSize, const FLinearColor& InColor)
{
	return EclipseMenuStyle::Text(*WidgetTree, InText, InSize, InColor);
}

void USettingsWidget::SelectTab(ESettingsTab InTab)
{
	const int32	TabIndex = static_cast<int32>(InTab);

	if (!this->TabSwitcher || !this->TabButtons.IsValidIndex(TabIndex))
	{
		return;
	}
	this->TabSwitcher->SetActiveWidgetIndex(TabIndex);

	const FLinearColor SelectedColor = FMath::Lerp(EclipseMenuStyle::Tile, AccentColor, 0.18f);
	const FLinearColor NormalColor = EclipseMenuStyle::Ink;

	for (int32 i = 0; i < this->TabButtons.Num(); ++i)
	{
		FButtonStyle Style = EclipseMenuStyle::Button(NormalColor, AccentColor);
		if (i == TabIndex)
		{
			Style.SetNormal(EclipseMenuStyle::Box(SelectedColor, 3.f, AccentColor, 1.f));
		}
		TabButtons[i]->SetStyle(Style);
	}
}

void USettingsWidget::LoadCurrentSettings()
{
	UGameUserSettings*	Settings = UGameUserSettings::GetGameUserSettings();

	if (!Settings)
	{
		return;
	}

	// UI를 채울 때 발생하는 선택 이벤트는 저장하지 않는다.
	TGuardValue<bool> RefreshGuard(bRefreshingSettings, true);

	// 디스플레이
	const bool bCanChangeWindow = !IsPlayingInEditor();
	if (this->WindowModeComboBox)
	{
		this->WindowModeComboBox->SetSelectedIndex(SettingsStyle::ToWindowModeIndex(Settings->GetFullscreenMode()));
		WindowModeComboBox->SetIsEnabled(bCanChangeWindow);
	}
	if (this->ResolutionComboBox)
	{
		const FIntPoint	CurrentResolution = Settings->GetScreenResolution();

		this->Resolutions.Reset();
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(this->Resolutions);
		this->Resolutions.AddUnique(CurrentResolution);
		this->Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
		{
			return A.X != B.X ? A.X > B.X : A.Y > B.Y;
		});

		this->ResolutionComboBox->ClearOptions();
		for (const FIntPoint& Resolution : this->Resolutions)
		{
			this->ResolutionComboBox->AddOption(SettingsStyle::ToResolutionString(Resolution));
		}
		this->ResolutionComboBox->SetSelectedOption(SettingsStyle::ToResolutionString(CurrentResolution));
		ResolutionComboBox->SetIsEnabled(bCanChangeWindow && Settings->GetFullscreenMode() != EWindowMode::WindowedFullscreen);
	}
	if (this->VSyncCheckBox)
	{
		this->VSyncCheckBox->SetIsChecked(Settings->IsVSyncEnabled());
	}

	if (const UUserAudioSettingsSubsystem* Audio = GetAudioSettings())
	{
		if (MasterVolumeSlider)
		{
			MasterVolumeSlider->SetValue(Audio->GetMasterVolume());
			UpdateVolumeText(MasterVolumeText, Audio->GetMasterVolume());
		}
		if (MusicVolumeSlider)
		{
			MusicVolumeSlider->SetValue(Audio->GetMusicVolume());
			UpdateVolumeText(MusicVolumeText, Audio->GetMusicVolume());
			if (!Audio->HasMusicSoundClass())
			{
				MusicVolumeSlider->SetToolTipText(FText::FromString(TEXT("배경음이 연결되면 저장된 음량이 적용됩니다.")));
			}
		}
		if (SFXVolumeSlider)
		{
			SFXVolumeSlider->SetValue(Audio->GetSFXVolume());
			UpdateVolumeText(SFXVolumeText, Audio->GetSFXVolume());
		}
	}

	if (QualityComboBox)
	{
		const int32 Quality = Settings->GetOverallScalabilityLevel();
		if (SettingsStyle::QualityOptions.IsValidIndex(Quality))
		{
			QualityComboBox->SetSelectedIndex(Quality);
		}
		else
		{
			const FString CustomQuality = TEXT("사용자 지정");
			if (QualityComboBox->FindOptionIndex(CustomQuality) == INDEX_NONE)
			{
				QualityComboBox->AddOption(CustomQuality);
			}
			QualityComboBox->SetSelectedOption(CustomQuality);
		}
	}

	if (FrameLimitComboBox)
	{
		FrameLimits = SettingsStyle::FrameLimitValues;
		FrameLimitComboBox->ClearOptions();
		for (const FString& Option : SettingsStyle::FrameLimitOptions)
		{
			FrameLimitComboBox->AddOption(Option);
		}

		const float CurrentLimit = Settings->GetFrameRateLimit();
		int32 FrameIndex = FrameLimits.IndexOfByKey(CurrentLimit);
		if (FrameIndex == INDEX_NONE)
		{
			FrameIndex = FrameLimits.Add(CurrentLimit);
			FrameLimitComboBox->AddOption(FString::SanitizeFloat(CurrentLimit));
		}
		FrameLimitComboBox->SetSelectedIndex(FrameIndex);
	}
}

UEnhancedInputUserSettings* USettingsWidget::GetInputUserSettings() const
{
	const UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetOwningLocalPlayer());
	if (!Subsystem) return nullptr;

	return Subsystem->GetUserSettings();
}

void USettingsWidget::RefreshKeyBindingList()
{
	if (!KeyBindingList) return;

	KeyBindingList->ClearChildren();
	KeyBindingSelectors.Reset();
	SetKeyBindingStatus(FText::GetEmpty(), false);

	UEnhancedInputUserSettings* Settings = GetInputUserSettings();
	if (!Settings)
	{
		SetKeyBindingStatus(FText::FromString(TEXT("입력 설정을 불러올 수 없습니다.")), true);
		return;
	}

	// 메인 메뉴에는 활성 게임플레이 IMC가 없으므로 표시할 매핑을 먼저 등록한다.
	const UInputMappingContext* Contexts[] = {DisplayMappingContext.LoadSynchronous(), TutorialMappingContext.LoadSynchronous()};
	TSet<FName> ListedMappings;
	for (const UInputMappingContext* Context : Contexts)
	{
		if (!Context) continue;

		Settings->RegisterInputMappingContext(Context);
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (!Mapping.IsPlayerMappable()) continue;

			const FName Name = Mapping.GetMappingName();
			if (ListedMappings.Contains(Name)) continue;

			const FPlayerKeyMapping* PlayerMapping = Settings->FindCurrentMappingForSlot(Name, EPlayerMappableKeySlot::First);
			if (!PlayerMapping) continue;
			ListedMappings.Add(Name);

			UKeyBindingSelector* Selector = WidgetTree->ConstructWidget<UKeyBindingSelector>();
			Selector->MappingName = Name;
			Selector->ActionLabel = PlayerMapping->GetDisplayName();
			Selector->SetAllowGamepadKeys(false);
			Selector->SetAllowModifierKeys(false);
			Selector->SetKeySelectionText(FText::FromString(TEXT("새 키 입력…")));
			Selector->SetButtonStyle(EclipseMenuStyle::Button(EclipseMenuStyle::Ink, AccentColor));

			FTextBlockStyle TextStyle;
			TextStyle.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15));
			TextStyle.SetColorAndOpacity(FSlateColor(EclipseMenuStyle::Paper));
			Selector->SetTextStyle(TextStyle);
			Selector->SetDisplayedKey(PlayerMapping->GetCurrentKey());
			Selector->OnBindingKeyChanged.BindUObject(this, &USettingsWidget::HandleBindingKeyChanged);
			KeyBindingSelectors.Add(Selector);
			AddRow(KeyBindingList, Selector->ActionLabel, Selector);
		}
	}

	if (KeyBindingSelectors.IsEmpty())
	{
		SetKeyBindingStatus(FText::FromString(TEXT("변경할 수 있는 단축키가 없습니다.")), true);
	}
}

void USettingsWidget::HandleBindingKeyChanged(UKeyBindingSelector& Selector, const FKey& NewKey)
{
	UEnhancedInputUserSettings* Settings = GetInputUserSettings();
	if (!Settings) return;

	const FPlayerKeyMapping* Mapping = Settings->FindCurrentMappingForSlot(Selector.MappingName, EPlayerMappableKeySlot::First);
	if (!Mapping) return;

	const FKey PreviousKey = Mapping->GetCurrentKey();
	if (PreviousKey == NewKey) return;

	if (!NewKey.IsValid() || NewKey == EKeys::Escape || NewKey == EKeys::F11 || GetDefault<UInputSettings>()->ConsoleKeys.Contains(NewKey))
	{
		Selector.SetDisplayedKey(PreviousKey);
		SetKeyBindingStatus(FText::FromString(TEXT("이 키는 단축키로 지정할 수 없습니다.")), true);
		return;
	}

	for (const UKeyBindingSelector* Other : KeyBindingSelectors)
	{
		if (Other == &Selector) continue;
		if (Other->GetSelectedKey().Key != NewKey) continue;

		Selector.SetDisplayedKey(PreviousKey);
		SetKeyBindingStatus(FText::Format(FText::FromString(TEXT("{0} 키는 이미 ‘{1}’에 사용 중입니다.")),
			NewKey.GetDisplayName(), Other->ActionLabel), true);
		return;
	}

	FMapPlayerKeyArgs Args;
	Args.MappingName = Selector.MappingName;
	Args.Slot = EPlayerMappableKeySlot::First;
	Args.NewKey = NewKey;

	FGameplayTagContainer FailureReason;
	Settings->MapPlayerKey(Args, FailureReason);
	if (!FailureReason.IsEmpty())
	{
		Selector.SetDisplayedKey(PreviousKey);
		SetKeyBindingStatus(FText::FromString(TEXT("단축키를 변경할 수 없습니다.")), true);
		return;
	}

	Settings->SaveSettings();
	SetKeyBindingStatus(FText::FromString(TEXT("단축키를 변경하고 저장했습니다.")), false);
}

void USettingsWidget::ResetKeyBindings()
{
	UEnhancedInputUserSettings* Settings = GetInputUserSettings();
	if (!Settings) return;
	if (KeyBindingSelectors.IsEmpty()) return;

	FGameplayTagContainer FailureReason;
	for (UKeyBindingSelector* Selector : KeyBindingSelectors)
	{
		FMapPlayerKeyArgs Args;
		Args.MappingName = Selector->MappingName;
		Settings->ResetAllPlayerKeysInRow(Args, FailureReason);

		if (const FPlayerKeyMapping* Mapping = Settings->FindCurrentMappingForSlot(Args.MappingName, EPlayerMappableKeySlot::First))
		{
			Selector->SetDisplayedKey(Mapping->GetCurrentKey());
		}
	}

	Settings->SaveSettings();
	if (!FailureReason.IsEmpty())
	{
		SetKeyBindingStatus(FText::FromString(TEXT("일부 단축키를 복원할 수 없습니다.")), true);
		return;
	}

	SetKeyBindingStatus(FText::FromString(TEXT("기본 단축키를 복원했습니다.")), false);
}

void USettingsWidget::SetKeyBindingStatus(const FText& Message, bool bWarning)
{
	if (!KeyBindingStatus) return;

	KeyBindingStatus->SetText(Message);
	KeyBindingStatus->SetColorAndOpacity(FSlateColor(bWarning ? EclipseMenuStyle::Corona : EclipseMenuStyle::Mist));
}

void USettingsWidget::HandleWindowModeChanged(FString Option, ESelectInfo::Type)
{
	if (bRefreshingSettings) return;
	if (IsPlayingInEditor()) return;

	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return;

	const int32 Index = SettingsStyle::WindowModeOptions.IndexOfByKey(Option);
	if (Index == INDEX_NONE) return;

	const EWindowMode::Type Mode = SettingsStyle::ToWindowMode(Index);
	Settings->SetFullscreenMode(Mode);
	ResolutionComboBox->SetIsEnabled(Mode != EWindowMode::WindowedFullscreen);
	Settings->ApplyResolutionSettings(false);
	Settings->ConfirmVideoMode();
	Settings->SaveSettings();
}

void USettingsWidget::HandleResolutionChanged(FString Option, ESelectInfo::Type)
{
	if (bRefreshingSettings) return;
	if (IsPlayingInEditor()) return;

	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return;

	const int32 Index = ResolutionComboBox->FindOptionIndex(Option);
	if (!Resolutions.IsValidIndex(Index)) return;

	Settings->SetScreenResolution(Resolutions[Index]);
	Settings->ApplyResolutionSettings(false);
	Settings->ConfirmVideoMode();
	Settings->SaveSettings();
}

void USettingsWidget::HandleVSyncChanged(bool bChecked)
{
	if (bRefreshingSettings) return;

	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return;

	Settings->SetVSyncEnabled(bChecked);
	Settings->ApplyNonResolutionSettings();
	Settings->SaveSettings();
}

void USettingsWidget::HandleQualityChanged(FString Option, ESelectInfo::Type)
{
	if (bRefreshingSettings) return;

	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return;

	const int32 Index = SettingsStyle::QualityOptions.IndexOfByKey(Option);
	if (Index == INDEX_NONE) return;

	Settings->SetOverallScalabilityLevel(Index);
	QualityComboBox->RemoveOption(TEXT("사용자 지정"));
	Settings->ApplyNonResolutionSettings();
	Settings->SaveSettings();
}

void USettingsWidget::HandleFrameLimitChanged(FString Option, ESelectInfo::Type)
{
	if (bRefreshingSettings) return;

	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return;

	const int32 Index = FrameLimitComboBox->FindOptionIndex(Option);
	if (!FrameLimits.IsValidIndex(Index)) return;

	Settings->SetFrameRateLimit(FrameLimits[Index]);
	Settings->ApplyNonResolutionSettings();
	Settings->SaveSettings();
}

UUserAudioSettingsSubsystem* USettingsWidget::GetAudioSettings() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	return World->GetSubsystem<UUserAudioSettingsSubsystem>();
}

bool USettingsWidget::IsPlayingInEditor() const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	return World->WorldType == EWorldType::PIE;
}

void USettingsWidget::HandleCloseClicked()
{
	this->CloseSettings();
}

void USettingsWidget::CloseSettings()
{
	RemoveFromParent();
	this->OnSettingsClosed.Broadcast();
}

void USettingsWidget::HandleDisplayTabClicked()
{
	this->SelectTab(ESettingsTab::Display);
}

void USettingsWidget::HandleAudioTabClicked()
{
	this->SelectTab(ESettingsTab::Audio);
}

void USettingsWidget::HandleGraphicsTabClicked()
{
	this->SelectTab(ESettingsTab::Graphics);
}

void USettingsWidget::HandleControlsTabClicked()
{
	this->SelectTab(ESettingsTab::Controls);
}

void USettingsWidget::HandleMasterVolumeChanged(float InValue)
{
	if (bRefreshingSettings) return;

	UUserAudioSettingsSubsystem* Audio = GetAudioSettings();
	if (!Audio) return;

	Audio->SetMasterVolume(InValue);
	UpdateVolumeText(MasterVolumeText, Audio->GetMasterVolume());
}

void USettingsWidget::HandleMusicVolumeChanged(float InValue)
{
	if (bRefreshingSettings) return;

	UUserAudioSettingsSubsystem* Audio = GetAudioSettings();
	if (!Audio) return;

	Audio->SetMusicVolume(InValue);
	UpdateVolumeText(MusicVolumeText, Audio->GetMusicVolume());
}

void USettingsWidget::HandleSFXVolumeChanged(float InValue)
{
	if (bRefreshingSettings) return;

	UUserAudioSettingsSubsystem* Audio = GetAudioSettings();
	if (!Audio) return;

	Audio->SetSFXVolume(InValue);
	UpdateVolumeText(SFXVolumeText, Audio->GetSFXVolume());
}

void USettingsWidget::UpdateVolumeText(UTextBlock* InText, const float InValue)
{
	if (InText)
	{
		InText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(InValue * 100.f))));
	}
}
