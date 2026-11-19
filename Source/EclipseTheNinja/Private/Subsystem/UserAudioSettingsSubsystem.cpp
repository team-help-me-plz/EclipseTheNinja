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
	GConfig->GetFloat(AudioSection, TEXT("AmbientVolume"), AmbientVolume, GGameUserSettingsIni);
	MasterVolume = FMath::Clamp(MasterVolume, 0.f, 1.f);
	MusicVolume = FMath::Clamp(MusicVolume, 0.f, 1.f);
	SFXVolume = FMath::Clamp(SFXVolume, 0.f, 1.f);
	AmbientVolume = FMath::Clamp(AmbientVolume, 0.f, 1.f);

	FAudioDeviceHandle AudioDevice = InWorld.GetAudioDevice();
	if (!AudioDevice.IsValid()) return;

	// 분류 클래스가 Master 자체로 잘못 지정되면 그 슬라이더가 모든 소리를 줄이므로 쓰지 않는다.
	USoundClass* MasterClass = GetDefault<UAudioSettings>()->GetDefaultSoundClass();
	MusicSoundClass = MusicSoundClassAsset.LoadSynchronous();
	if (MusicSoundClass == MasterClass)
	{
		MusicSoundClass = nullptr;
	}
	SFXSoundClass = SFXSoundClassAsset.LoadSynchronous();
	if (SFXSoundClass == MasterClass)
	{
		SFXSoundClass = nullptr;
	}
	AmbientSoundClass = AmbientSoundClassAsset.LoadSynchronous();
	if (AmbientSoundClass == MasterClass)
	{
		AmbientSoundClass = nullptr;
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

void UUserAudioSettingsSubsystem::SetAmbientVolume(float InValue)
{
	SetVolume(TEXT("AmbientVolume"), AmbientVolume, InValue);
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

	// 분류 슬라이더는 자기 SoundClass(와 그 자식)만 조절한다. 마스터 음량은 위에서 전체에 곱해진다
	if (SFXSoundClass)
	{
		AudioDevice->SetSoundMixClassOverride(VolumeMix, SFXSoundClass, SFXVolume, 1.f, 0.f, true);
	}
	if (MusicSoundClass)
	{
		AudioDevice->SetSoundMixClassOverride(VolumeMix, MusicSoundClass, MusicVolume, 1.f, 0.f, true);
	}
	if (AmbientSoundClass)
	{
		AudioDevice->SetSoundMixClassOverride(VolumeMix, AmbientSoundClass, AmbientVolume, 1.f, 0.f, true);
	}
}
