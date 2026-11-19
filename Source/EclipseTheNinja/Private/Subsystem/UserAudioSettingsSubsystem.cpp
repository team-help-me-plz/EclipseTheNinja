#include "Subsystem/UserAudioSettingsSubsystem.h"

#include "AudioDevice.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace
{
	constexpr TCHAR AudioSection[] = TEXT("EclipseTheNinja.Audio");
}

bool UUserAudioSettingsSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UUserAudioSettingsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	GConfig->GetFloat(AudioSection, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
	GConfig->GetFloat(AudioSection, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
	GConfig->GetFloat(AudioSection, TEXT("SFXVolume"), SFXVolume, GGameUserSettingsIni);
	MasterVolume = FMath::Clamp(MasterVolume, 0.f, 1.f);
	MusicVolume = FMath::Clamp(MusicVolume, 0.f, 1.f);
	SFXVolume = FMath::Clamp(SFXVolume, 0.f, 1.f);

	FAudioDeviceHandle AudioDevice = InWorld.GetAudioDevice();
	if (!AudioDevice.IsValid()) return;

	MusicSoundClass = MusicSoundClassAsset.LoadSynchronous();
	if (MusicSoundClass == GetDefault<UAudioSettings>()->GetDefaultSoundClass())
	{
		MusicSoundClass = nullptr;
	}

	// 월드당 한 번만 믹스를 활성화해 슬라이더를 움직여도 참조 수가 늘지 않게 한다.
	VolumeMix = NewObject<USoundMix>(this);
	ApplyVolumes();
	AudioDevice->PushSoundMixModifier(VolumeMix);
}

void UUserAudioSettingsSubsystem::Deinitialize()
{
	if (VolumeMix)
	{
		FAudioDeviceHandle AudioDevice = GetWorld()->GetAudioDevice();
		if (AudioDevice.IsValid())
		{
			AudioDevice->ClearSoundMixModifier(VolumeMix);
		}
	}

	Super::Deinitialize();
}

void UUserAudioSettingsSubsystem::SetMasterVolume(float InValue)
{
	SetVolume(TEXT("MasterVolume"), MasterVolume, InValue);
}

void UUserAudioSettingsSubsystem::SetMusicVolume(float InValue)
{
	SetVolume(TEXT("MusicVolume"), MusicVolume, InValue);
}

void UUserAudioSettingsSubsystem::SetSFXVolume(float InValue)
{
	SetVolume(TEXT("SFXVolume"), SFXVolume, InValue);
}

void UUserAudioSettingsSubsystem::SetVolume(const TCHAR* Key, float& Volume, float InValue)
{
	const float NewVolume = FMath::Clamp(InValue, 0.f, 1.f);
	if (Volume == NewVolume) return;

	Volume = NewVolume;
	ApplyVolumes();

	GConfig->SetFloat(AudioSection, Key, Volume, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UUserAudioSettingsSubsystem::ApplyVolumes()
{
	if (!VolumeMix) return;

	FAudioDeviceHandle AudioDevice = GetWorld()->GetAudioDevice();
	if (!AudioDevice.IsValid()) return;

	AudioDevice->SetTransientPrimaryVolume(MasterVolume);

	// 현재 효과음은 엔진 기본 클래스를 사용한다. 배경음 클래스는 독립적으로 조절한다.
	if (USoundClass* SFXClass = GetDefault<UAudioSettings>()->GetDefaultSoundClass())
	{
		AudioDevice->SetSoundMixClassOverride(VolumeMix, SFXClass, SFXVolume, 1.f, 0.f, false);
	}
	if (MusicSoundClass)
	{
		AudioDevice->SetSoundMixClassOverride(VolumeMix, MusicSoundClass, MusicVolume, 1.f, 0.f, true);
	}
}
