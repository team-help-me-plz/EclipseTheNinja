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

	// 메뉴가 열리기 전에 마지막으로 쓴 입력 장치를 알려 준다(첫 입력 전에도 포커스 색이 맞게).
	static void SetUsingGamepad(bool bInUsingGamepad);

	// 바인딩 없이 버튼 목록에 포커스 모양을 입힌다. 게임패드 포커스는 전용 색, 마우스는 호버 모양.
	// FocusBrush를 주면 기본 붉은 포커스 대신 그 브러시를 쓴다.
	void UpdateButtons(
		const UUserWidget* Owner,
		TConstArrayView<UButton*> Buttons,
		bool bActive = true,
		const FSlateBrush* FocusBrush = nullptr
	);
private:
	// 포커스 여부에 맞춰 버튼 스타일을 원래 스타일 기준으로 다시 입힌다. bUseFocusColor면 게임패드 포커스에 전용 색.
	void ApplyButtonStyle(
		UButton& Button,
		bool bFocused,
		bool bUseFocusColor,
		const FSlateBrush* FocusBrush = nullptr
	);

	TSharedPtr<IInputProcessor> InputProcessor;
	TMap<TWeakObjectPtr<UButton>, FButtonStyle> ButtonStyles;

	// 직전 Update에서 포커스된 컨트롤. 키보드·게임패드로 다른 컨트롤로 옮겨 가면 호버 사운드를 낸다
	TWeakObjectPtr<UWidget> LastFocusedControl;
};
