


#include "Widgets/UIMessageEntryWidget.h"

#include "Components/TextBlock.h"
#include "TimerManager.h"

void UUIMessageEntryWidget::SetEntryText(const FText& InText, const float InMessageHoldTime)
{
	this->EntryText->SetText(InText);

	GetWorld()->GetTimerManager().SetTimer(
		this->MessageHoldTimerHandle,
		this,
		&UUIMessageEntryWidget::PlayDisapear,
		InMessageHoldTime,
		false
	);
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
