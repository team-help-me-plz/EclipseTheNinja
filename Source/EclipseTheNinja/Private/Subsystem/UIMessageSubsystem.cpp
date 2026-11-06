


#include "Subsystem/UIMessageSubsystem.h"
#include "Widgets/UIMessageWidget.h"

#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

void UUIMessageSubsystem::RegisterUIMessageWidget(UUIMessageWidget* InWidget)
{
	this->TargetUIMessageWidget = InWidget;
}

void UUIMessageSubsystem::RegisterUIMessage(APlayerController* InPC, UUIMessageWidget* InWidget)
{
	if (!InPC)
	{
		return;
	}
	if (ULocalPlayer* LocalPlayer = InPC->GetLocalPlayer())
	{
		if (UUIMessageSubsystem* Subsystem = LocalPlayer->GetSubsystem<UUIMessageSubsystem>())
		{
			Subsystem->RegisterUIMessageWidget(InWidget);
		}
	}
}

void UUIMessageSubsystem::ShowMessageText(const FText& InText, float InHoldDuration)
{
	if (!this->TargetUIMessageWidget)
	{
		return ;
	}
	this->TargetUIMessageWidget->MakeMessage(InText, InHoldDuration);
}

void UUIMessageSubsystem::ShowMessage(APlayerController* InPC, const FText& InText, float InHoldDuration)
{
	if (!InPC)
	{
		return ;
	}
	if (ULocalPlayer* LocalPlayer = InPC->GetLocalPlayer())
	{
		if (UUIMessageSubsystem* Subsystem = LocalPlayer->GetSubsystem<UUIMessageSubsystem>())
		{
			Subsystem->ShowMessageText(InText, InHoldDuration);
		}
	}
}
