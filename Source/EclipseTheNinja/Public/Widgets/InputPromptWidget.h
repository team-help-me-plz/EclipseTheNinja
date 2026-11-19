#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "Widgets/InputGlyphSet.h"
#include "InputPromptWidget.generated.h"

class UInputAction;
class UInputMappingContext;
class UTextBlock;
class UImage;
class UDataTable;

/** Device-aware caption binding for the existing UMG interaction/QTE badge. */
UCLASS()
class ECLIPSETHENINJA_API UInputPromptWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Input Prompt")
	void SetPromptAction(UInputAction* Action, const FText& KeyboardFallback);

	void SetQTEPromptAction(UInputAction* Action, const FText& KeyboardFallback);

	UFUNCTION(BlueprintPure, Category = "Input Prompt")
	FText GetPromptLabel(bool bGamepad) const;
	static FName GetIconNameForLabel(const FText& Label);

	static FText ResolveActionLabel(const UObject* WorldContext, APlayerController* Controller,
		const UInputAction* Action, const UInputMappingContext* MappingContext,
		const FText& KeyboardFallback, bool bGamepad);
	static FKey ResolveActionKey(const UObject* WorldContext, APlayerController* Controller,
		const UInputAction* Action, const UInputMappingContext* MappingContext,
		const FText& KeyboardFallback, bool bGamepad);
	static FText FormatKey(const FKey& Key);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Prompt")
	TObjectPtr<UInputAction> PromptAction;
	// Used when the action's context is temporarily removed by a gameplay input lock.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Prompt")
	TObjectPtr<UInputMappingContext> FallbackMappingContext;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Prompt")
	FText KeyboardFallbackText = FText::FromString(TEXT("E"));
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KeyText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> GamepadIcon;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> KeyboardBackground;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Prompt")
	TObjectPtr<UDataTable> GamepadIconTable;

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RefreshLabel();

	UPROPERTY()
	TSoftObjectPtr<UInputGlyphSet> GlyphSet{FSoftObjectPath(TEXT("/Game/UI/InputGlyphs/DA_InputGlyphs.DA_InputGlyphs"))};

	UPROPERTY(Transient)
	TObjectPtr<UInputGlyphSet> LoadedGlyphSet;

	FText LastLabel;
	float OriginalWidth = 0.0f;
};
