#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/InputGlyphSet.h"
#include "InputGlyphWidget.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UEnhancedInputUserSettings;
class UInputAction;
class UImage;
class UInputGlyphSubsystem;

UCLASS()
class ECLIPSETHENINJA_API UInputGlyphWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "Input Glyph")
	void SetInputAction(const UInputAction* Action);

	UFUNCTION(BlueprintPure, Category = "Input Glyph")
	FKey GetDisplayedKey() const { return DisplayedKey; }

private:
	UFUNCTION()
	void RefreshGlyph();

	UFUNCTION()
	void HandleSettingsChanged(UEnhancedInputUserSettings* Settings);

	FKey ResolveKey() const;

	UPROPERTY(Transient)
	TObjectPtr<const UInputAction> InputAction;

	UPROPERTY(Transient)
	TObjectPtr<UImage> GlyphImage;

	// 소프트 참조도 쿠킹 시 추적되며 데이터 에셋이 텍스처를 보유한다.
	UPROPERTY()
	TSoftObjectPtr<UInputGlyphSet> GlyphSet = TSoftObjectPtr<UInputGlyphSet>(FSoftObjectPath(TEXT("/Game/UI/InputGlyphs/DA_InputGlyphs.DA_InputGlyphs")));

	UPROPERTY(Transient)
	TObjectPtr<UInputGlyphSet> LoadedGlyphSet;

	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> EnhancedInput;
	TWeakObjectPtr<UEnhancedInputUserSettings> InputSettings;
	TWeakObjectPtr<UInputGlyphSubsystem> DeviceSubsystem;
	FDelegateHandle DeviceChangedHandle;
	FKey DisplayedKey;
};
