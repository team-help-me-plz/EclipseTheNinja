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
	float GetAmbientVolume() const { return AmbientVolume; }
	bool HasMusicSoundClass() const { return MusicSoundClass != nullptr; }
	bool HasAmbientSoundClass() const { return AmbientSoundClass != nullptr; }

	void SetMasterVolume(float InValue);
	void SetMusicVolume(float InValue);
	void SetSFXVolume(float InValue);
	void SetAmbientVolume(float InValue);

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	void SetVolume(const TCHAR* Key, float& Volume, float InValue);
	void ApplyVolumes();

	// 분류별 SoundClass를 DefaultGame.ini에서 연결한다. 각 클래스는 프로젝트 기본 클래스 SC_Master의 자식이다.
	// 클래스를 지정하지 않은 사운드는 SC_Master에 속해 마스터 음량만 따른다.
	UPROPERTY(Config)
	TSoftObjectPtr<USoundClass> MusicSoundClassAsset;

	UPROPERTY(Config)
	TSoftObjectPtr<USoundClass> SFXSoundClassAsset;

	UPROPERTY(Config)
	TSoftObjectPtr<USoundClass> AmbientSoundClassAsset;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> SFXSoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> AmbientSoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> VolumeMix;

	float MasterVolume = 1.f;
	float MusicVolume = 1.f;
	float SFXVolume = 1.f;
	float AmbientVolume = 1.f;
};
