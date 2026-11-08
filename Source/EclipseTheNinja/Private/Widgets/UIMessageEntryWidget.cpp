


#include "Widgets/UIMessageEntryWidget.h"

#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/RichTextBlockImageDecorator.h"
#include "TimerManager.h"
#include "Character/PlayerCharacterController.h"
#include "Widgets/InputPromptWidget.h"
#include "InputAction.h"
#include "InputMappingContext.h"

void UUIMessageEntryWidget::SetEntryText(const FText& InText, const float InMessageHoldTime)
{
	OriginalMessageText = InText;
	bHasDeviceState = false;
	RefreshDeviceText();

	GetWorld()->GetTimerManager().SetTimer(
		this->MessageHoldTimerHandle,
		this,
		&UUIMessageEntryWidget::PlayDisapear,
		InMessageHoldTime,
		false
	);
}

FText UUIMessageEntryWidget::GetDeviceMessageText(const FText& Text, bool bGamepad) const
{
	return FormatDeviceMessage(Text, bGamepad, false);
}

namespace
{
	FText EscapeMessageMarkup(const FText& Text)
	{
		FString Value = Text.ToString();
		Value.ReplaceInline(TEXT("&"), TEXT("&amp;"));
		Value.ReplaceInline(TEXT("<"), TEXT("&lt;"));
		Value.ReplaceInline(TEXT(">"), TEXT("&gt;"));
		return FText::FromString(Value);
	}
}

FText UUIMessageEntryWidget::FormatIconLabel(const FText& Label) const
{
	FText IconLabel = Label;
	FString Suffix;
	// Directional stick instructions keep their words after the inline L-stick icon.
	const FString Value = Label.ToString();
	if (Value.StartsWith(TEXT("왼쪽 스틱")))
	{
		IconLabel = FText::FromString(TEXT("왼쪽 스틱"));
		Suffix = Value.Mid(FString(TEXT("왼쪽 스틱")).Len());
	}
	const FName IconName = UInputPromptWidget::GetIconNameForLabel(IconLabel);
	if (GamepadIconTable && !IconName.IsNone()
		&& GamepadIconTable->FindRow<FRichImageRow>(IconName, TEXT("UIMessage"), false))
	{
		return FText::FromString(FString::Printf(TEXT("<img id=\"%s\"></>"), *IconName.ToString())
			+ EscapeMessageMarkup(FText::FromString(Suffix)).ToString());
	}
	return EscapeMessageMarkup(Label);
}

FText UUIMessageEntryWidget::GetDeviceMessageRichText(const FText& Text, bool bGamepad) const
{
	return FormatDeviceMessage(Text, bGamepad, true);
}

FText UUIMessageEntryWidget::FormatDeviceMessage(const FText& Text, bool bGamepad, bool bWithIcons) const
{
	if (!bGamepad) return bWithIcons ? EscapeMessageMarkup(Text) : Text;
	const FText* Template = GamepadMessageTemplates.Find(Text.ToString());
	if (!Template) return bWithIcons ? EscapeMessageMarkup(Text) : Text;
	FFormatNamedArguments Arguments;
	for (const auto& Pair : GamepadTextArguments)
		Arguments.Add(Pair.Key, bWithIcons ? FormatIconLabel(Pair.Value) : Pair.Value);
	const FString TemplateString = Template->ToString();
	for (const auto& Pair : MessageInputActions)
	{
		if (!TemplateString.Contains(TEXT("{") + Pair.Key + TEXT("}"))) continue;
		const FText Label = UInputPromptWidget::ResolveActionLabel(this, GetOwningPlayer(), Pair.Value,
			MessageMappingContext, FText::GetEmpty(), true);
		if (Label.IsEmpty()) return Text;
		Arguments.Add(Pair.Key, bWithIcons ? FormatIconLabel(Label) : Label);
	}
	return FText::Format(bWithIcons ? EscapeMessageMarkup(*Template) : *Template, Arguments);
}

void UUIMessageEntryWidget::RefreshDeviceText()
{
	if (!EntryText) return;
	const APlayerCharacterController* Controller = Cast<APlayerCharacterController>(GetOwningPlayer());
	const bool bGamepad = Controller && Controller->IsUsingGamepad();
	if (bHasDeviceState && bLastUsingGamepad == bGamepad) return;
	bHasDeviceState = true;
	bLastUsingGamepad = bGamepad;
	const bool bShowRich = bGamepad && RichEntryText && GamepadIconTable;
	EntryText->SetVisibility(bShowRich ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (RichEntryText)
	{
		RichEntryText->SetVisibility(bShowRich ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShowRich) RichEntryText->SetText(GetDeviceMessageRichText(OriginalMessageText, true));
	}
	if (!bShowRich) EntryText->SetText(GetDeviceMessageText(OriginalMessageText, bGamepad));
}

void UUIMessageEntryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshDeviceText();
	// Keep the existing EntryText fade animation driving the inline-icon version.
	if (RichEntryText && EntryText) RichEntryText->SetRenderOpacity(EntryText->GetRenderOpacity());
}

void UUIMessageEntryWidget::PlayDisapear()
{
	PlayAnimation(this->TextDisapearAnim);
}

void UUIMessageEntryWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	if (this->TextDisapearAnim && Animation == TextDisapearAnim)
	{
		RemoveFromParent();
	}
}
