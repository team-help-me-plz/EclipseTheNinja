#include "Widgets/MenuInputFeedback.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

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

void FMenuInputFeedback::Update(UUserWidget* Owner, const TArray<FMenuFocusFeedbackBinding>& Bindings, bool bActive)
{
	for (const FMenuFocusFeedbackBinding& Binding : Bindings)
	{
		UWidget* Control = Owner->GetWidgetFromName(Binding.ControlName);
		const UComboBoxString* Combo = Cast<UComboBoxString>(Control);
		const bool bFocused = bActive && Control && Control->GetIsEnabled() && Control->IsVisible()
			&& ((Combo && Combo->IsOpen()) || (MenuFeedback::bPointer ? Control->IsHovered()
				: (Control->HasUserFocus(Owner->GetOwningPlayer()) || Control->HasUserFocusedDescendants(Owner->GetOwningPlayer()))));
		if (UButton* Button = Cast<UButton>(Control))
		{
			const FButtonStyle& BaseStyle = ButtonStyles.FindOrAdd(Button, Button->GetStyle());
			FButtonStyle Style = BaseStyle;
			if (bFocused) Style.SetNormal(BaseStyle.Hovered);
			Button->SetStyle(Style);
		}
		if (UWidget* Highlight = Owner->GetWidgetFromName(Binding.HighlightName))
			Highlight->SetVisibility(bFocused ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (UWidget* Icon = Owner->GetWidgetFromName(Binding.AcceptIconName))
			Icon->SetVisibility(bFocused && MenuFeedback::bGamepad ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UWidget* Hints = Owner->GetWidgetFromName(TEXT("GamepadHints")))
		Hints->SetVisibility(bActive && MenuFeedback::bGamepad ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (UWidget* Hints = Owner->GetWidgetFromName(TEXT("KeyboardHints")))
		Hints->SetVisibility(bActive && !MenuFeedback::bGamepad ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	for (FName Name : {FName(TEXT("PreviousTabHint")), FName(TEXT("NextTabHint"))})
		if (UWidget* Hint = Owner->GetWidgetFromName(Name))
			Hint->SetVisibility(bActive && MenuFeedback::bGamepad ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}
