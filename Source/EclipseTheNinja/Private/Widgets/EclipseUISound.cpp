#include "EclipseUISound.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Framework/Application/SlateApplication.h"
#include "Sound/SoundBase.h"
#include "Widgets/EclipseMenuWidget.h"

namespace
{
	FSlateSound MakeSlateSound(USoundBase* SoundAsset)
	{
		FSlateSound Sound;

		if (SoundAsset)
		{
			Sound.SetResourceObject(SoundAsset);
		}
		return Sound;
	}

	const UEclipseMenuWidget* AsMenuWidget(const UUserWidget* Owner)
	{
		return Cast<UEclipseMenuWidget>(Owner);
	}
}

void EclipseUISound::ApplyButtonSounds(FButtonStyle& Style, const UUserWidget* Owner)
{
	const UEclipseMenuWidget* Menu = AsMenuWidget(Owner);
	if (!Menu)
	{
		return;
	}

	Style.SetHoveredSound(MakeSlateSound(Menu->GetHoverSound()));
	Style.SetPressedSound(MakeSlateSound(Menu->GetSelectSound()));
}

void EclipseUISound::ApplyButtonSoundsToTree(UUserWidget* Owner)
{
	if (!Owner || !Owner->WidgetTree)
	{
		return;
	}

	Owner->WidgetTree->ForEachWidget([Owner](UWidget* Widget)
	{
		UButton* Button = Cast<UButton>(Widget);
		if (!Button)
		{
			return;
		}

		FButtonStyle Style = Button->GetStyle();
		ApplyButtonSounds(Style, Owner);
		Button->SetStyle(Style);
	});
}

void EclipseUISound::PlayHoverSound(const UUserWidget* Owner)
{
	const UEclipseMenuWidget* Menu = AsMenuWidget(Owner);
	if (!Menu || !Menu->GetHoverSound() || !FSlateApplication::IsInitialized())
	{
		return;
	}

	FSlateApplication::Get().PlaySound(MakeSlateSound(Menu->GetHoverSound()));
}
