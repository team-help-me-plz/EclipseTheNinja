#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UserAudioSettingsSubsystem.generated.h"

class USoundClass;
class USoundMix;

UCLASS(Config = Game)
class UUserAudioSettingsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	float GetMasterVolume() const { return MasterVolume; }
	float GetMusicVolume() const { return MusicVolume; }
	float GetSFXVolume() const { return SFXVolume; }
	bool HasMusicSoundClass() const { return MusicSoundClass != nullptr; }

	void SetMasterVolume(float InValue);
	void SetMusicVolume(float InValue);
	void SetSFXVolume(float InValue);

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	void SetVolume(const TCHAR* Key, float& Volume, float InValue);
	void ApplyVolumes();

	// 배경음 에셋에 지정한 전용 SoundClass를 DefaultGame.ini에서 연결한다.
	UPROPERTY(Config)
	TSoftObjectPtr<USoundClass> MusicSoundClassAsset;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> VolumeMix;

	float MasterVolume = 1.f;
	float MusicVolume = 1.f;
	float SFXVolume = 1.f;
};
