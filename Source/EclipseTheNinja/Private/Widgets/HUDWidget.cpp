


#include "Widgets/HUDWidget.h"
#include "Widgets/PlayerStatWidget.h"
#include "Widgets/AbilityCooldownContainerWidget.h"
#include "Widgets/CinematicSkipWidget.h"
#include "Character/PlayerCharacterController.h"
#include "Components/PanelWidget.h"
#include "GameFramework/PlayerController.h"

void UHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->OnPossessedPawnChanged.AddUniqueDynamic(this, &UHUDWidget::HandlePossessedPawnChanged);
		InitWidget(PlayerController->GetPawn());
	}

	// 컷신이 HUD보다 먼저 시작했어도 컨트롤러가 기억한 건너뛰기 안내 상태를 받는다
	if (APlayerCharacterController* CharacterController = Cast<APlayerCharacterController>(GetOwningPlayer()))
	{
		CharacterController->RegisterHUDWidget(this);
	}
}

void UHUDWidget::NativeDestruct()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &UHUDWidget::HandlePossessedPawnChanged);
	}
	if (APlayerCharacterController* CharacterController = Cast<APlayerCharacterController>(GetOwningPlayer()))
	{
		CharacterController->UnregisterHUDWidget(this);
	}

	Super::NativeDestruct();
}

void UHUDWidget::SetCinematicSkipAvailable(bool bAvailable)
{
	if (!CinematicSkipWidget)
	{
		return;
	}

	CinematicSkipWidget->SetSkipAvailable(bAvailable);

	if (bAvailable)
	{
		// 안내와 같은 패널에 있는 다른 HUD 위젯을 숨긴다
		if (UPanelWidget* Parent = CinematicSkipWidget->GetParent())
		{
			for (UWidget* Child : Parent->GetAllChildren())
			{
				if (IsValid(Child) && Child != CinematicSkipWidget && Child->IsVisible() && !CinematicHiddenWidgets.Contains(Child))
				{
					CinematicHiddenWidgets.Add(Child, Child->GetVisibility());
					Child->SetVisibility(ESlateVisibility::Hidden);
				}
			}
		}
		return;
	}

	for (const TPair<TWeakObjectPtr<UWidget>, ESlateVisibility>& Pair : CinematicHiddenWidgets)
	{
		if (Pair.Key.IsValid())
		{
			Pair.Key->SetVisibility(Pair.Value);
		}
	}
	CinematicHiddenWidgets.Reset();
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
	if (AbilityCooldownContainerWidget)
	{
		AbilityCooldownContainerWidget->InitWidget(InPawn);
	}
}
