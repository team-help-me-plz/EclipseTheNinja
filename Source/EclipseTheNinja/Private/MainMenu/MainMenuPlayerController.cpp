

#include "MainMenu/MainMenuPlayerController.h"
#include "MainMenu/MainMenuGameMode.h"
#include "Widgets/MainMenuWidget.h"

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	TSubclassOf<UMainMenuWidget>	WidgetClass = UMainMenuWidget::StaticClass();
	if (const AMainMenuGameMode* GameMode = GetWorld()->GetAuthGameMode<AMainMenuGameMode>())
	{
		if (GameMode->GetMainMenuWidgetClass())
		{
			WidgetClass = GameMode->GetMainMenuWidgetClass();
		}
	}

	this->MainMenuWidget = CreateWidget<UMainMenuWidget>(this, WidgetClass);
	if (!this->MainMenuWidget)
	{
		return;
	}
	this->MainMenuWidget->AddToViewport();

	FInputModeUIOnly	InputMode;
	InputMode.SetWidgetToFocus(this->MainMenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	this->bShowMouseCursor = true;
}
