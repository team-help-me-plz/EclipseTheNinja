


#include "Widgets/UIMessageWidget.h"
#include "Subsystem/UIMessageSubsystem.h"

#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UUIMessageWidget::MakeMessage(const FText& InText)
{
	if (!this->UIMessageVerticalBox || !this->MessageEntryClass || !GetOwningPlayer())
	{
		return;
	}
	UUIMessageEntryWidget* NewMessageEntry = CreateWidget<UUIMessageEntryWidget>(GetOwningPlayer(), this->MessageEntryClass);
	if (!NewMessageEntry)
	{
		return ;
	}
	NewMessageEntry->SetEntryText(InText, this->MessageHoldDuration);
	UVerticalBoxSlot* EntrySlot = this->UIMessageVerticalBox->AddChildToVerticalBox(NewMessageEntry);
	if (EntrySlot)
	{
		EntrySlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		EntrySlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

void UUIMessageWidget::NativeConstruct()
{
	APlayerController*	PC = GetOwningPlayer();

	UUIMessageSubsystem::RegisterUIMessage(PC, this);
}
