#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/PrimaryAssetId.h"
#include "TimerManager.h"
#include "EclipseStageManagerSubsystem.generated.h"

class SWidget;
class UEclipseStageDefinition;
class UNiagaraComponent;
class UNiagaraSystem;
class UTexture2D;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnEclipseStageLoaded, const UEclipseStageDefinition* /*정의가 없는 맵이면 nullptr*/);

enum class EEclipseStageLoadState : uint8
{
	Unloaded,
	Loading,
	Loaded,
};


/**
 * 맵이 열리면 World Settings의 StageDefinition을 비동기로 불러오고, 끝나면 OnStageLoaded를 알린다
 * (라이라 ULyraExperienceManagerComponent).
 *  1) 로딩 화면을 띄우고 정의·공용 묶음의 Game 번들을 비동기 로드한다
 *  2) 플레이어 카메라가 생기면 그 앞에서 Effects를 실제 크기로 재생하고, 쓰는 텍스처의 밉을 모두 올린다
 *  3) 텍스처가 다 올라오면(또는 시간 제한) 이펙트를 지우고 로딩 화면을 내린 뒤 OnStageLoaded를 알린다
 * 맵이 닫히면 이 맵에서 불러온 것을 놓는다.
 */
UCLASS()
class ECLIPSETHENINJA_API UEclipseStageManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	bool IsStageLoaded() const { return LoadState == EEclipseStageLoadState::Loaded; }
	const UEclipseStageDefinition* GetStageDefinition() const { return StageDefinition; }

	// 이미 로드됐으면 바로 호출하고, 아니면 로드가 끝날 때 호출한다
	void CallOrRegister_OnStageLoaded(FOnEclipseStageLoaded::FDelegate&& Delegate);

private:
	void StartStageLoad(const TSoftObjectPtr<UEclipseStageDefinition>& Definition);
	void OnStageAssetsLoaded();

	// GameData·스테이지 정의·공용 묶음의 Effects (중복 제거)
	TArray<UNiagaraSystem*> CollectEffects() const;

	// 프리웜: 플레이어 카메라를 기다렸다가 카메라 앞에서 실제 크기로 재생하고 텍스처 밉이 다 올라올 때까지 기다린다
	void BeginWarmup();
	void TickWarmup();
	void SpawnWarmupEffects(const FVector& Location, const FRotator& Rotation);
	void FinishWarmup(const TCHAR* Reason);
	void ClearWarmupEffects();

	void ShowLoadingScreen();
	void HideLoadingScreen();
	// 로딩 화면을 서서히 지운다 (끝나면 HideLoadingScreen)
	void FadeOutLoadingScreen();
	void TickLoadingFade();

	UPROPERTY(Transient)
	TObjectPtr<const UEclipseStageDefinition> StageDefinition;

	// 이 맵에서 불러온 Primary Asset (정의 + 공용 묶음)
	TArray<FPrimaryAssetId> LoadedIds;

	EEclipseStageLoadState LoadState = EEclipseStageLoadState::Unloaded;
	FOnEclipseStageLoaded OnStageLoaded;
	double LoadStartTime = 0.0;

	// 프리웜 중인 이펙트와 밉을 올리는 중인 텍스처
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> WarmupComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> WarmupTextures;

	FTimerHandle WarmupTimer;
	double WarmupBeginTime = 0.0;   // 플레이어 카메라를 기다리기 시작한 시각
	double WarmupSpawnTime = 0.0;   // 이펙트를 재생한 시각 (0이면 아직 재생 전)
	int32 WarmupEffectCount = 0;

	TSharedPtr<SWidget> LoadingScreen;
	FTimerHandle LoadingFadeTimer;
	double LoadingFadeStartTime = 0.0;
};
