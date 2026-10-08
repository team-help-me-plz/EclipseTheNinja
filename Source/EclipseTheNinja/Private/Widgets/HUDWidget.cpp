


#include "Widgets/HUDWidget.h"
#include "Widgets/PlayerStatWidget.h"
#include "GameFramework/PlayerController.h"

void UHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->OnPossessedPawnChanged.AddUniqueDynamic(this, &UHUDWidget::HandlePossessedPawnChanged);
		InitWidget(PlayerController->GetPawn());
	}
}

void UHUDWidget::NativeDestruct()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &UHUDWidget::HandlePossessedPawnChanged);
	}

	Super::NativeDestruct();
}

void UHUDWidget::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	InitWidget(NewPawn);
}

void UHUDWidget::InitWidget(APawn* InPawn)
{
	if (PlayerStatWidget)
	{
		PlayerStatWidget->InitWidget(InPawn);
	}
}
