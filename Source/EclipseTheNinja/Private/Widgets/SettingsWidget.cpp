

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
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Styling/CoreStyle.h"

namespace SettingsStyle
{
	const FLinearColor	PanelColor(0.012f, 0.012f, 0.015f, 0.98f);
	const FLinearColor	RowColor(1.f, 1.f, 1.f, 0.03f);
	const FLinearColor	SubTextColor(0.6f, 0.6f, 0.6f, 1.f);

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

	FButtonStyle	MakeButtonStyle(const FLinearColor& InNormal, const FLinearColor& InAccent)
	{
		FButtonStyle	Style;
		Style.SetNormal(MakeBoxBrush(InNormal, 4.f, FLinearColor(1.f, 1.f, 1.f, 0.12f), 1.f));
		Style.SetHovered(MakeBoxBrush(FLinearColor(InAccent.R, InAccent.G, InAccent.B, 0.25f), 4.f, InAccent, 1.f));
		Style.SetPressed(MakeBoxBrush(FLinearColor(InAccent.R, InAccent.G, InAccent.B, 0.5f), 4.f, InAccent, 1.f));
		Style.SetNormalPadding(FMargin(0.f));
		Style.SetPressedPadding(FMargin(0.f));
		return Style;
	}

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

FReply USettingsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		this->CloseSettings();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USettingsWidget::BuildDefaultLayout()
{
	UOverlay*	Root = this->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	this->WidgetTree->RootWidget = Root;

	// 뒤쪽 메인 화면을 어둡게 가리는 막
	UImage*		Dim = this->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DimImage"));
	Dim->SetBrush(SettingsStyle::MakeBoxBrush(FLinearColor(0.f, 0.f, 0.f, 0.75f)));
	if (UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim))
	{
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);
	}

	USizeBox*	PanelBox = this->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelBox"));
	PanelBox->SetWidthOverride(960.f);
	PanelBox->SetHeightOverride(640.f);
	if (UOverlaySlot* PanelSlot = Root->AddChildToOverlay(PanelBox))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
	}

	UBorder*	Panel = this->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrush(SettingsStyle::MakeBoxBrush(SettingsStyle::PanelColor, 8.f, FLinearColor(this->AccentColor.R, this->AccentColor.G, this->AccentColor.B, 0.6f), 1.f));
	Panel->SetPadding(FMargin(32.f, 24.f));
	PanelBox->AddChild(Panel);

	UVerticalBox*	PanelContent = this->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PanelContent"));
	Panel->SetContent(PanelContent);

	// 헤더
	if (UVerticalBoxSlot* HeaderSlot = PanelContent->AddChildToVerticalBox(this->MakeText(FText::FromString(TEXT("설정")), 32)))
	{
		HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
	}

	// 탭 버튼
	UHorizontalBox*	TabBar = this->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TabBar"));
	if (UVerticalBoxSlot* TabBarSlot = PanelContent->AddChildToVerticalBox(TabBar))
	{
		TabBarSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
	}

	const FText	TabLabels[] = {
		FText::FromString(TEXT("디스플레이")),
		FText::FromString(TEXT("오디오")),
		FText::FromString(TEXT("그래픽")),
		FText::FromString(TEXT("조작키")),
	};
	for (const FText& TabLabel : TabLabels)
	{
		UButton*	TabButton = this->MakeTextButton(TabLabel, 0.f);
		this->TabButtons.Add(TabButton);
		if (UHorizontalBoxSlot* TabSlot = TabBar->AddChildToHorizontalBox(TabButton))
		{
			TabSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			TabSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
	}
	this->TabButtons[static_cast<int32>(ESettingsTab::Display)]->OnClicked.AddUniqueDynamic(this, &USettingsWidget::HandleDisplayTabClicked);
	this->TabButtons[static_cast<int32>(ESettingsTab::Audio)]->OnClicked.AddUniqueDynamic(this, &USettingsWidget::HandleAudioTabClicked);
	this->TabButtons[static_cast<int32>(ESettingsTab::Graphics)]->OnClicked.AddUniqueDynamic(this, &USettingsWidget::HandleGraphicsTabClicked);
	this->TabButtons[static_cast<int32>(ESettingsTab::Controls)]->OnClicked.AddUniqueDynamic(this, &USettingsWidget::HandleControlsTabClicked);

	// 탭 페이지 (ESettingsTab 순서와 동일하게 추가)
	this->TabSwitcher = this->WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("TabSwitcher"));
	if (UVerticalBoxSlot* SwitcherSlot = PanelContent->AddChildToVerticalBox(this->TabSwitcher))
	{
		SwitcherSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// 디스플레이
	{
		UVerticalBox*	Page = this->MakePage(FText::FromString(TEXT("화면 표시 방식과 해상도를 설정합니다")));

		this->WindowModeComboBox = this->WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
		for (const FString& Option : SettingsStyle::WindowModeOptions)
		{
			this->WindowModeComboBox->AddOption(Option);
		}
		this->AddRow(Page, FText::FromString(TEXT("화면 모드")), this->WindowModeComboBox);

		this->ResolutionComboBox = this->WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
		this->AddRow(Page, FText::FromString(TEXT("해상도")), this->ResolutionComboBox);

		this->VSyncCheckBox = this->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
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

		this->QualityComboBox = this->WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
		for (const FString& Option : SettingsStyle::QualityOptions)
		{
			this->QualityComboBox->AddOption(Option);
		}
		this->AddRow(Page, FText::FromString(TEXT("전체 품질")), this->QualityComboBox);

		this->FrameLimitComboBox = this->WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
		for (const FString& Option : SettingsStyle::FrameLimitOptions)
		{
			this->FrameLimitComboBox->AddOption(Option);
		}
		this->AddRow(Page, FText::FromString(TEXT("프레임 제한")), this->FrameLimitComboBox);
	}

	// 조작키
	{
		UVerticalBox*	Page = this->MakePage(FText::FromString(TEXT("현재 키 배치 (키 변경 기능은 추후 추가 예정)")));

		UScrollBox*	Scroll = this->WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		if (UVerticalBoxSlot* ScrollSlot = Page->AddChildToVerticalBox(Scroll))
		{
			ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		this->KeyBindingList = this->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("KeyBindingList"));
		Scroll->AddChild(this->KeyBindingList);
	}

	// 하단 버튼
	UHorizontalBox*	Footer = this->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Footer"));
	if (UVerticalBoxSlot* FooterSlot = PanelContent->AddChildToVerticalBox(Footer))
	{
		FooterSlot->SetHorizontalAlignment(HAlign_Right);
		FooterSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
	}

	UButton*	ApplyButton = this->MakeTextButton(FText::FromString(TEXT("적용")), 140.f);
	ApplyButton->OnClicked.AddUniqueDynamic(this, &USettingsWidget::HandleApplyClicked);
	if (UHorizontalBoxSlot* ApplySlot = Footer->AddChildToHorizontalBox(ApplyButton))
	{
		ApplySlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}

	UButton*	CloseButton = this->MakeTextButton(FText::FromString(TEXT("닫기")), 140.f);
	CloseButton->OnClicked.AddUniqueDynamic(this, &USettingsWidget::HandleCloseClicked);
	Footer->AddChildToHorizontalBox(CloseButton);
}

UVerticalBox* USettingsWidget::MakePage(const FText& InHeader)
{
	UVerticalBox*	Page = this->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	this->TabSwitcher->AddChild(Page);

	if (UVerticalBoxSlot* HeaderSlot = Page->AddChildToVerticalBox(this->MakeText(InHeader, 14, SettingsStyle::SubTextColor)))
	{
		HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}
	return Page;
}

void USettingsWidget::AddRow(UVerticalBox* InPage, const FText& InLabel, UWidget* InValue)
{
	UBorder*	RowBorder = this->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	RowBorder->SetBrush(SettingsStyle::MakeBoxBrush(SettingsStyle::RowColor, 4.f));
	RowBorder->SetPadding(FMargin(16.f, 8.f));

	UHorizontalBox*	Row = this->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RowBorder->SetContent(Row);

	if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(this->MakeText(InLabel, 18)))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	USizeBox*	ValueBox = this->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ValueBox->SetWidthOverride(360.f);
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
	Slider->SetSliderBarColor(FLinearColor(1.f, 1.f, 1.f, 0.2f));
	Slider->SetSliderHandleColor(this->AccentColor);
	if (UHorizontalBoxSlot* SliderSlot = ValueRow->AddChildToHorizontalBox(Slider))
	{
		SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SliderSlot->SetVerticalAlignment(VAlign_Center);
	}

	USizeBox*	TextBox = this->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	TextBox->SetWidthOverride(64.f);
	OutValueText = this->MakeText(FText::GetEmpty(), 16, SettingsStyle::SubTextColor);
	OutValueText->SetJustification(ETextJustify::Right);
	TextBox->AddChild(OutValueText);
	if (UHorizontalBoxSlot* TextSlot = ValueRow->AddChildToHorizontalBox(TextBox))
	{
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}

	this->AddRow(InPage, InLabel, ValueRow);
	return Slider;
}

UButton* USettingsWidget::MakeTextButton(const FText& InLabel, const float InWidth)
{
	UButton*	Button = this->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetStyle(SettingsStyle::MakeButtonStyle(FLinearColor(1.f, 1.f, 1.f, 0.04f), this->AccentColor));

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
	UTextBlock*	Text = this->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(InText);
	Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", InSize));
	Text->SetColorAndOpacity(FSlateColor(InColor));
	return Text;
}

void USettingsWidget::SelectTab(ESettingsTab InTab)
{
	const int32	TabIndex = static_cast<int32>(InTab);

	if (!this->TabSwitcher || !this->TabButtons.IsValidIndex(TabIndex))
	{
		return;
	}
	this->TabSwitcher->SetActiveWidgetIndex(TabIndex);

	const FLinearColor	SelectedColor(this->AccentColor.R, this->AccentColor.G, this->AccentColor.B, 0.4f);
	const FLinearColor	NormalColor(1.f, 1.f, 1.f, 0.04f);

	for (int32 i = 0; i < this->TabButtons.Num(); ++i)
	{
		this->TabButtons[i]->SetStyle(SettingsStyle::MakeButtonStyle(i == TabIndex ? SelectedColor : NormalColor, this->AccentColor));
	}
}

void USettingsWidget::LoadCurrentSettings()
{
	UGameUserSettings*	Settings = UGameUserSettings::GetGameUserSettings();

	if (!Settings)
	{
		return;
	}

	// 디스플레이
	if (this->WindowModeComboBox)
	{
		this->WindowModeComboBox->SetSelectedIndex(SettingsStyle::ToWindowModeIndex(Settings->GetFullscreenMode()));
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
	}
	if (this->VSyncCheckBox)
	{
		this->VSyncCheckBox->SetIsChecked(Settings->IsVSyncEnabled());
	}

	// 오디오
	if (this->MasterVolumeSlider)
	{
		this->MasterVolumeSlider->SetValue(this->MasterVolume);
		this->UpdateVolumeText(this->MasterVolumeText, this->MasterVolume);
	}
	if (this->MusicVolumeSlider)
	{
		this->MusicVolumeSlider->SetValue(this->MusicVolume);
		this->UpdateVolumeText(this->MusicVolumeText, this->MusicVolume);
	}
	if (this->SFXVolumeSlider)
	{
		this->SFXVolumeSlider->SetValue(this->SFXVolume);
		this->UpdateVolumeText(this->SFXVolumeText, this->SFXVolume);
	}

	// 그래픽 (사용자 지정 품질(-1)이면 에픽으로 표시)
	if (this->QualityComboBox)
	{
		const int32	Quality = Settings->GetOverallScalabilityLevel();
		this->QualityComboBox->SetSelectedIndex(Quality >= 0 ? Quality : 3);
	}
	if (this->FrameLimitComboBox)
	{
		const int32	FrameIndex = SettingsStyle::FrameLimitValues.IndexOfByKey(Settings->GetFrameRateLimit());
		this->FrameLimitComboBox->SetSelectedIndex(FrameIndex != INDEX_NONE ? FrameIndex : SettingsStyle::FrameLimitValues.Num() - 1);
	}
}

void USettingsWidget::RefreshKeyBindingList()
{
	if (!this->KeyBindingList)
	{
		return;
	}
	this->KeyBindingList->ClearChildren();

	const UInputMappingContext*	MappingContext = this->DisplayMappingContext.LoadSynchronous();
	if (!MappingContext)
	{
		this->KeyBindingList->AddChildToVerticalBox(this->MakeText(FText::FromString(TEXT("표시할 입력 매핑이 없습니다")), 16, SettingsStyle::SubTextColor));
		return;
	}

	for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
	{
		if (!Mapping.Action)
		{
			continue;
		}
		const FText	ActionName = Mapping.Action->ActionDescription.IsEmpty()
			? FText::FromString(Mapping.Action->GetName())
			: Mapping.Action->ActionDescription;

		this->AddRow(this->KeyBindingList, ActionName, this->MakeText(Mapping.Key.GetDisplayName(), 18, this->AccentColor));
	}
}

void USettingsWidget::HandleApplyClicked()
{
	UGameUserSettings*	Settings = UGameUserSettings::GetGameUserSettings();

	if (!Settings)
	{
		return;
	}

	const int32	WindowModeIndex = this->WindowModeComboBox->GetSelectedIndex();
	if (WindowModeIndex != INDEX_NONE)
	{
		Settings->SetFullscreenMode(SettingsStyle::ToWindowMode(WindowModeIndex));
	}

	const int32	ResolutionIndex = this->ResolutionComboBox->GetSelectedIndex();
	if (this->Resolutions.IsValidIndex(ResolutionIndex))
	{
		Settings->SetScreenResolution(this->Resolutions[ResolutionIndex]);
	}

	Settings->SetVSyncEnabled(this->VSyncCheckBox->IsChecked());

	const int32	QualityIndex = this->QualityComboBox->GetSelectedIndex();
	if (QualityIndex != INDEX_NONE)
	{
		Settings->SetOverallScalabilityLevel(QualityIndex);
	}

	const int32	FrameIndex = this->FrameLimitComboBox->GetSelectedIndex();
	if (SettingsStyle::FrameLimitValues.IsValidIndex(FrameIndex))
	{
		Settings->SetFrameRateLimit(SettingsStyle::FrameLimitValues[FrameIndex]);
	}

	// 적용 후 GameUserSettings.ini에 저장
	Settings->ApplySettings(false);

	this->ApplyAudioSettings();
}

void USettingsWidget::ApplyAudioSettings()
{
	if (!this->VolumeSoundMix)
	{
		return;
	}

	const TPair<USoundClass*, float>	ClassVolumes[] = {
		{ this->MasterSoundClass, this->MasterVolume },
		{ this->MusicSoundClass, this->MusicVolume },
		{ this->SFXSoundClass, this->SFXVolume },
	};
	for (const TPair<USoundClass*, float>& ClassVolume : ClassVolumes)
	{
		if (ClassVolume.Key)
		{
			UGameplayStatics::SetSoundMixClassOverride(this, this->VolumeSoundMix, ClassVolume.Key, ClassVolume.Value, 1.f, 0.f, true);
		}
	}
	UGameplayStatics::PushSoundMixModifier(this, this->VolumeSoundMix);
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
	this->MasterVolume = InValue;
	this->UpdateVolumeText(this->MasterVolumeText, InValue);
}

void USettingsWidget::HandleMusicVolumeChanged(float InValue)
{
	this->MusicVolume = InValue;
	this->UpdateVolumeText(this->MusicVolumeText, InValue);
}

void USettingsWidget::HandleSFXVolumeChanged(float InValue)
{
	this->SFXVolume = InValue;
	this->UpdateVolumeText(this->SFXVolumeText, InValue);
}

void USettingsWidget::UpdateVolumeText(UTextBlock* InText, const float InValue)
{
	if (InText)
	{
		InText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(InValue * 100.f))));
	}
}
