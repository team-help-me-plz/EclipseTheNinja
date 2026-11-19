#include "EclipseUISound.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Framework/Application/SlateApplication.h"
#include "Sound/SoundBase.h"

namespace
{
	// 에셋 시트(시스템/UI)에서 정한 사운드. /Game/Audio는 패키징 시 항상 쿡된다(DefaultGame.ini)
	const TCHAR* HoverSoundPath = TEXT("/Game/Audio/UI_Click_04.UI_Click_04");
	const TCHAR* SelectSoundPath = TEXT("/Game/Audio/UI_Click_23.UI_Click_23");

	FSlateSound MakeSlateSound(const TCHAR* Path)
	{
		FSlateSound Sound;
		USoundBase* SoundAsset = LoadObject<USoundBase>(nullptr, Path);

		if (SoundAsset)
		{
			Sound.SetResourceObject(SoundAsset);
		}
		return Sound;
	}
}

void EclipseUISound::ApplyButtonSounds(FButtonStyle& Style)
{
	Style.SetHoveredSound(MakeSlateSound(HoverSoundPath));
	Style.SetPressedSound(MakeSlateSound(SelectSoundPath));
}

void EclipseUISound::ApplyButtonSoundsToTree(UUserWidget* Owner)
{
	if (!Owner || !Owner->WidgetTree)
	{
		return;
	}

	Owner->WidgetTree->ForEachWidget([](UWidget* Widget)
	{
		UButton* Button = Cast<UButton>(Widget);
		if (!Button)
		{
			return;
		}

		FButtonStyle Style = Button->GetStyle();
		ApplyButtonSounds(Style);
		Button->SetStyle(Style);
	});
}

void EclipseUISound::PlayHoverSound()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	FSlateApplication::Get().PlaySound(MakeSlateSound(HoverSoundPath));
}
