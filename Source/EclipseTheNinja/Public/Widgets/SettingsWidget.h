
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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
class ECLIPSETHENINJA_API USettingsWidget : public UUserWidget
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
	void	HandleMasterVolumeChanged(float InValue);

	UFUNCTION()
	void	HandleMusicVolumeChanged(float InValue);

	UFUNCTION()
	void	HandleSFXVolumeChanged(float InValue);

	// 현재 GameUserSettings 값을 UI에 반영
	void	LoadCurrentSettings();

	// 메인 메뉴에서도 플레이 중인 매핑을 등록할 수 있게 한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Controls")
	TSoftObjectPtr<UInputMappingContext>	DisplayMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Controls")
	TSoftObjectPtr<UInputMappingContext> TutorialMappingContext = TSoftObjectPtr<UInputMappingContext>(
		FSoftObjectPath(TEXT("/Game/Blueprint/Week_4/ParryTest/IMC_ParryTest.IMC_ParryTest")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Style")
	FLinearColor	AccentColor = FLinearColor::FromSRGBColor(FColor(196, 150, 102));

	UPROPERTY(Transient)
	TObjectPtr<UWidgetSwitcher>	TabSwitcher = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>>	TabButtons;

	// 디스플레이
	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString>	WindowModeComboBox = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString>	ResolutionComboBox = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox>		VSyncCheckBox = nullptr;

	// 오디오
	UPROPERTY(Transient)
	TObjectPtr<USlider>		MasterVolumeSlider = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USlider>		MusicVolumeSlider = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USlider>		SFXVolumeSlider = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock>	MasterVolumeText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock>	MusicVolumeText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock>	SFXVolumeText = nullptr;

	// 그래픽
	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString>	QualityComboBox = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString>	FrameLimitComboBox = nullptr;

	// 조작키
	UPROPERTY(Transient)
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
