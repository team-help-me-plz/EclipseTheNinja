
#pragma once

#include "CoreMinimal.h"
#include "Widgets/EclipseMenuWidget.h"
#include "Widgets/MenuInputFeedback.h"
#include "SettingsWidget.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class USlider;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;
class UInputMappingContext;
class UEnhancedInputUserSettings;
class UKeyBindingSelector;
class UUserAudioSettingsSubsystem;
class IInputProcessor;
class FSettingsMenuInputProcessor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsClosed);

UENUM(BlueprintType)
enum class ESettingsTab : uint8
{
	Display,
	Audio,
	Graphics,
	Controls,
	Max UMETA(Hidden)
};

/**
 * 환경설정 위젯
 * 디스플레이 / 오디오 / 그래픽 / 조작키 탭으로 구성되며, WBP 없이 C++ 레이아웃을 구성한다.
 */
UCLASS()
class ECLIPSETHENINJA_API USettingsWidget : public UEclipseMenuWidget
{
	GENERATED_BODY()

public:
	virtual bool	Initialize() override;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void	SelectTab(ESettingsTab InTab);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void	CloseSettings();

	UPROPERTY(BlueprintAssignable, Category = "Settings")
	FOnSettingsClosed	OnSettingsClosed;

protected:
	virtual void	NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings|Input")
	TArray<FMenuFocusFeedbackBinding> FocusFeedbackBindings;
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& InGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply) override;

	UFUNCTION()
	void	HandleDisplayTabClicked();

	UFUNCTION()
	void	HandleAudioTabClicked();

	UFUNCTION()
	void	HandleGraphicsTabClicked();

	UFUNCTION()
	void	HandleControlsTabClicked();

	UFUNCTION()
	void HandleWindowModeChanged(FString Option, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleResolutionChanged(FString Option, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleVSyncChanged(bool bChecked);

	UFUNCTION()
	void HandleQualityChanged(FString Option, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleFrameLimitChanged(FString Option, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void	HandleCloseClicked();

	UFUNCTION()
	void HandleApplyClicked();

	UFUNCTION()
	void	HandleMasterVolumeChanged(float InValue);

	UFUNCTION()
	void	HandleMusicVolumeChanged(float InValue);

	UFUNCTION()
	void	HandleSFXVolumeChanged(float InValue);

	UFUNCTION()
	void	HandleAmbientVolumeChanged(float InValue);

	// 현재 GameUserSettings 값을 UI에 반영
	void	LoadCurrentSettings();

	// 메인 메뉴에서도 플레이 중인 매핑을 등록할 수 있게 한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Controls")
	TSoftObjectPtr<UInputMappingContext>	DisplayMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Style")
	FLinearColor	AccentColor = FLinearColor::FromSRGBColor(FColor(196, 150, 102));
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings|Style")
	FLinearColor SelectedTabColor = FLinearColor(0.35f, 0.045f, 0.04f, 1.f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings|Style")
	FLinearColor IdleTabColor = FLinearColor(0.04f, 0.04f, 0.045f, 1.f);

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher>	TabSwitcher = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>>	TabButtons;

	// 디스플레이
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UComboBoxString>	WindowModeComboBox = nullptr;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UComboBoxString>	ResolutionComboBox = nullptr;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox>		VSyncCheckBox = nullptr;

	// 오디오
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<USlider>		MasterVolumeSlider = nullptr;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<USlider>		MusicVolumeSlider = nullptr;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<USlider>		SFXVolumeSlider = nullptr;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<USlider>		AmbientVolumeSlider = nullptr;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock>	MasterVolumeText = nullptr;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock>	MusicVolumeText = nullptr;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock>	SFXVolumeText = nullptr;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock>	AmbientVolumeText = nullptr;

	// 그래픽
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UComboBoxString>	QualityComboBox = nullptr;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UComboBoxString>	FrameLimitComboBox = nullptr;

	// 조작키
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox>	KeyBindingList = nullptr;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> KeyBindingStatus;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UKeyBindingSelector>> KeyBindingSelectors;

	UEnhancedInputUserSettings* GetInputUserSettings() const;
	void HandleBindingKeyChanged(UKeyBindingSelector& Selector, const FKey& NewKey);
	void SetKeyBindingStatus(const FText& Message, bool bWarning);

	UFUNCTION()
	void ResetKeyBindings();

	void BindDesignerControls();
	FMenuInputFeedback InputFeedback;
	friend class FSettingsMenuInputProcessor;
	bool HandleGamepadKey(const FKeyEvent& Event);
	bool OwnsGamepadFocus(int32 UserIndex) const;
	bool HasOpenCombo() const;
	void FocusControl(UWidget* Widget);
	void FocusSelectedTab();
	void MoveContentFocus(int32 Direction);
	TArray<UWidget*> GetContentControls() const;
	TSharedPtr<IInputProcessor> GamepadInputProcessor;
	ESettingsTab CurrentTab = ESettingsTab::Display;
	bool bTabFocus = true;
	bool bConsumeAcceptRelease = false;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> FooterButtons;
	void		BuildDefaultLayout();
	UVerticalBox*	MakePage(const FText& InHeader);
	void		AddRow(UVerticalBox* InPage, const FText& InLabel, UWidget* InValue);
	USlider*	AddVolumeRow(UVerticalBox* InPage, const FText& InLabel, TObjectPtr<UTextBlock>& OutValueText);
	UComboBoxString* MakeSettingsComboBox();

	UFUNCTION()
	UWidget* MakeComboOption(FString Option);
	UButton*	MakeTextButton(const FText& InLabel, const float InWidth);
	UTextBlock*	MakeText(const FText& InText, const int32 InSize, const FLinearColor& InColor = FLinearColor::White);
	void		RefreshKeyBindingList();
	void		UpdateVolumeText(UTextBlock* InText, const float InValue);

	TArray<FIntPoint>	Resolutions;

	UUserAudioSettingsSubsystem* GetAudioSettings() const;
	bool IsPlayingInEditor() const;
	TArray<float> FrameLimits;
	bool bRefreshingSettings = false;
};
