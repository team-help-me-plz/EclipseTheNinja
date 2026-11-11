#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "MenuInputFeedback.generated.h"

class UUserWidget;
class UButton;
class UWidget;
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

	// 직전 Update에서 포커스된 컨트롤. 키보드·게임패드로 다른 컨트롤로 옮겨 가면 호버 사운드를 낸다
	TWeakObjectPtr<UWidget> LastFocusedControl;
};
