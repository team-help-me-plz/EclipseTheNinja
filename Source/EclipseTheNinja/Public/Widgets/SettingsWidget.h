
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
class USoundClass;
class USoundMix;

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
 * 설정 창 위젯 (초안)
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
	virtual FReply	NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void	HandleDisplayTabClicked();

	UFUNCTION()
	void	HandleAudioTabClicked();

	UFUNCTION()
	void	HandleGraphicsTabClicked();

	UFUNCTION()
	void	HandleControlsTabClicked();

	UFUNCTION()
	void	HandleApplyClicked();

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
	void	ApplyAudioSettings();

	// 조작키 탭에 표시할 입력 매핑 (키 변경 기능은 추후 구현)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Controls")
	TSoftObjectPtr<UInputMappingContext>	DisplayMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Input/IMC_Character.IMC_Character")));

	// 볼륨 조절에 사용할 사운드 믹스 / 클래스 (지정되지 않으면 값만 저장)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundMix>	VolumeSoundMix = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundClass>	MasterSoundClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundClass>	MusicSoundClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundClass>	SFXSoundClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Style")
	FLinearColor	AccentColor = FLinearColor(0.6f, 0.015f, 0.025f, 1.f);

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
	void		BuildDefaultLayout();
	UVerticalBox*	MakePage(const FText& InHeader);
	void		AddRow(UVerticalBox* InPage, const FText& InLabel, UWidget* InValue);
	USlider*	AddVolumeRow(UVerticalBox* InPage, const FText& InLabel, TObjectPtr<UTextBlock>& OutValueText);
	UButton*	MakeTextButton(const FText& InLabel, const float InWidth);
	UTextBlock*	MakeText(const FText& InText, const int32 InSize, const FLinearColor& InColor = FLinearColor::White);
	void		RefreshKeyBindingList();
	void		UpdateVolumeText(UTextBlock* InText, const float InValue);

	TArray<FIntPoint>	Resolutions;

	float	MasterVolume = 1.f;
	float	MusicVolume = 1.f;
	float	SFXVolume = 1.f;
};
