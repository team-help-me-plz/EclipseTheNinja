#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "MenuInputFeedback.generated.h"

class UUserWidget;
class UButton;
class IInputProcessor;

/** Designer-owned visuals associated with one focusable menu control. */
USTRUCT(BlueprintType)
struct FMenuFocusFeedbackBinding
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ControlName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName HighlightName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AcceptIconName;
};

/** Tracks Slate UIOnly input, and applies focus state to existing UMG visuals. */
class FMenuInputFeedback
{
public:
	void Start();
	void Stop();
	void Update(UUserWidget* Owner, const TArray<FMenuFocusFeedbackBinding>& Bindings, bool bActive = true);
	static bool IsUsingGamepad();
private:
	TSharedPtr<IInputProcessor> InputProcessor;
	TMap<TWeakObjectPtr<UButton>, FButtonStyle> ButtonStyles;
};
