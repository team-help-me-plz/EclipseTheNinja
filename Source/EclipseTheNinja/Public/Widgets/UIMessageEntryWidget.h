

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UIMessageEntryWidget.generated.h"

class UTextBlock;
class UInputAction;
class UInputMappingContext;
class URichTextBlock;
class UDataTable;

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UUIMessageEntryWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	//void	
	void	SetEntryText(const FText& InText, const float InMessageHoldTime);
	void	PlayDisapear();
	UFUNCTION(BlueprintPure, Category = "UIMessage|Input")
	FText GetDeviceMessageText(const FText& Text, bool bGamepad) const;
	UFUNCTION(BlueprintPure, Category = "UIMessage|Input")
	FText GetDeviceMessageRichText(const FText& Text, bool bGamepad) const;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Exact keyboard message -> gamepad sentence with named action placeholders.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UIMessage|Input", meta = (MultiLine = "true"))
	TMap<FString, FText> GamepadMessageTemplates;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UIMessage|Input")
	TMap<FString, TObjectPtr<UInputAction>> MessageInputActions;
	// Directional explanations such as "left stick down", which are not separate IAs.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UIMessage|Input")
	TMap<FString, FText> GamepadTextArguments;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UIMessage|Input")
	TObjectPtr<UInputMappingContext> MessageMappingContext;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UIMessage|Input")
	TObjectPtr<UDataTable> GamepadIconTable;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<URichTextBlock> RichEntryText;
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock>			EntryText = nullptr;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation>	TextDisapearAnim = nullptr;

private:
	FText FormatDeviceMessage(const FText& Text, bool bGamepad, bool bWithIcons) const;
	FText FormatIconLabel(const FText& Label) const;
	void RefreshDeviceText();
	FText OriginalMessageText;
	bool bHasDeviceState = false;
	bool bLastUsingGamepad = false;
	FTimerHandle	MessageHoldTimerHandle;

};
