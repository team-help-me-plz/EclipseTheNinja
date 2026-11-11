#include "Widgets/MenuInputFeedback.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "EclipseUISound.h"
#include "EclipseMenuStyle.h"
#include "GameFramework/PlayerController.h"

namespace MenuFeedback
{
	bool bGamepad = false;
	bool bPointer = false;
	class FDeviceInputProcessor : public IInputProcessor
	{
	public:
		virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
		virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
		{
			bGamepad = Event.GetKey().IsGamepadKey();
			bPointer = false;
			return false;
		}
		virtual bool HandleAnalogInputEvent(FSlateApplication&, const FAnalogInputEvent& Event) override
		{
			if (Event.GetKey().IsGamepadKey() && FMath::Abs(Event.GetAnalogValue()) > 0.25f)
			{
				bGamepad = true;
				bPointer = false;
			}
			return false;
		}
		virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent& Event) override
		{
			if (Event.GetCursorDelta().SizeSquared() > 4.0f) { bGamepad = false; bPointer = true; }
			return false;
		}
		virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent&) override
		{
			bGamepad = false;
			bPointer = true;
			return false;
		}
	};

	// 호버 브러시 모양(둥근 정도 등)은 두고 채움·테두리만 포커스 색으로 바꾼다.
	FSlateBrush MakeFocusBrush(const FSlateBrush& Base)
	{
		FSlateBrush Brush = Base;

		// 투명 버튼이면 색이 보이도록 둥근 상자로 그린다.
		if (Brush.DrawAs == ESlateBrushDrawType::NoDrawType)
		{
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		}

		Brush.TintColor = FSlateColor(EclipseMenuStyle::Ember);
		Brush.OutlineSettings.Color = FSlateColor(EclipseMenuStyle::Corona);
		Brush.OutlineSettings.Width = FMath::Max(Brush.OutlineSettings.Width, 2.f);

		return Brush;
	}
}

void FMenuInputFeedback::Start()
{
	if (FSlateApplication::IsInitialized() && !InputProcessor)
	{
		InputProcessor = MakeShared<MenuFeedback::FDeviceInputProcessor>();
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor, 0);
	}
}

void FMenuInputFeedback::Stop()
{
	if (FSlateApplication::IsInitialized() && InputProcessor)
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	InputProcessor.Reset();
	for (const auto& Entry : ButtonStyles)
		if (UButton* Button = Entry.Key.Get()) Button->SetStyle(Entry.Value);
	ButtonStyles.Reset();
}

bool FMenuInputFeedback::IsUsingGamepad() { return MenuFeedback::bGamepad; }

void FMenuInputFeedback::SetUsingGamepad(bool bInUsingGamepad)
{
	MenuFeedback::bGamepad = bInUsingGamepad;
	MenuFeedback::bPointer = !bInUsingGamepad;
}

void FMenuInputFeedback::UpdateButtons(
	const UUserWidget* Owner,
	TConstArrayView<UButton*> Buttons,
	bool bActive,
	const FSlateBrush* FocusBrush
)
{
	APlayerController* Player = Owner ? Owner->GetOwningPlayer() : nullptr;

	for (UButton* Button : Buttons)
	{
		if (!Button)
		{
			continue;
		}

		const bool bFocused = bActive
			&& Button->GetIsEnabled()
			&& Button->IsVisible()
			&& (MenuFeedback::bPointer
				? Button->IsHovered()
				: Button->HasUserFocus(Player));
		ApplyButtonStyle(*Button, bFocused, true, FocusBrush);
	}
}

void FMenuInputFeedback::ApplyButtonStyle(
	UButton& Button,
	bool bFocused,
	bool bUseFocusColor,
	const FSlateBrush* FocusBrush
)
{
	const FButtonStyle BaseStyle = ButtonStyles.FindOrAdd(&Button, Button.GetStyle());
	FButtonStyle Style = BaseStyle;

	// 게임패드 포커스는 호버와 다른 색으로 칠해 어느 버튼에 있는지 바로 보이게 한다.
	if (bFocused && bUseFocusColor && MenuFeedback::bGamepad)
	{
		const FSlateBrush AppliedBrush = FocusBrush
			? *FocusBrush
			: MenuFeedback::MakeFocusBrush(BaseStyle.Hovered);
		Style.SetNormal(AppliedBrush);
		Style.SetHovered(AppliedBrush);
	}
	else if (bFocused)
	{
		Style.SetNormal(BaseStyle.Hovered);
	}

	Button.SetStyle(Style);
}

void FMenuInputFeedback::Update(UUserWidget* Owner, const TArray<FMenuFocusFeedbackBinding>& Bindings, bool bActive)
{
	UWidget* FocusedControl = nullptr;

	for (const FMenuFocusFeedbackBinding& Binding : Bindings)
	{
		UWidget* Control = Owner->GetWidgetFromName(Binding.ControlName);
		const UComboBoxString* Combo = Cast<UComboBoxString>(Control);
		const bool bFocused = bActive && Control && Control->GetIsEnabled() && Control->IsVisible()
			&& ((Combo && Combo->IsOpen()) || (MenuFeedback::bPointer ? Control->IsHovered()
				: (Control->HasUserFocus(Owner->GetOwningPlayer()) || Control->HasUserFocusedDescendants(Owner->GetOwningPlayer()))));
		if (bFocused && !FocusedControl)
		{
			FocusedControl = Control;
		}
		if (UButton* Button = Cast<UButton>(Control))
		{
			// 바인딩 메뉴(설정)는 WBP 하이라이트가 포커스를 보여 주므로 전용 색은 쓰지 않는다.
			ApplyButtonStyle(*Button, bFocused, false);
		}
		if (UWidget* Highlight = Owner->GetWidgetFromName(Binding.HighlightName))
			Highlight->SetVisibility(bFocused ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (UWidget* Icon = Owner->GetWidgetFromName(Binding.AcceptIconName))
			Icon->SetVisibility(bFocused && MenuFeedback::bGamepad ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 마우스 호버는 버튼 스타일 사운드가 맡는다. 키보드·게임패드로 포커스를 옮긴 경우만 여기서 재생한다
	// (메뉴가 처음 열리며 기본 포커스를 받을 때는 이전 포커스가 없으므로 소리를 내지 않는다)
	const bool bMovedFocus = FocusedControl
		&& LastFocusedControl.IsValid()
		&& LastFocusedControl.Get() != FocusedControl;

	if (bMovedFocus && !MenuFeedback::bPointer)
	{
		EclipseUISound::PlayHoverSound();
	}
	LastFocusedControl = FocusedControl;
	if (UWidget* Hints = Owner->GetWidgetFromName(TEXT("GamepadHints")))
		Hints->SetVisibility(bActive && MenuFeedback::bGamepad ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (UWidget* Hints = Owner->GetWidgetFromName(TEXT("KeyboardHints")))
		Hints->SetVisibility(bActive && !MenuFeedback::bGamepad ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	for (FName Name : {FName(TEXT("PreviousTabHint")), FName(TEXT("NextTabHint"))})
		if (UWidget* Hint = Owner->GetWidgetFromName(Name))
			Hint->SetVisibility(bActive && MenuFeedback::bGamepad ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}
