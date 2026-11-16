

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "EclipseAssetManager.generated.h"

class UEclipseGameData;

/**
 * 프로젝트 에셋 매니저.
 *  - 엔진 시작 시 GameData를 동기 로드해 계속 들고 있는다 (경로 설정 없이 EclipseGameData 타입 에셋을 찾는다)
 *  - GetAsset/GetSubclass : 소프트 참조를 꺼낸다. 메모리에 없으면 동기 로드한다
 *    -LogAssetLoads 실행 인자를 주면 동기 로드를 시간과 함께 로그로 남긴다 (빠진 프리로드 찾기)
 * DefaultEngine.ini의 AssetManagerClassName으로 등록한다.
 */
UCLASS(Config = Game)
class ECLIPSETHENINJA_API UEclipseAssetManager : public UAssetManager
{
	GENERATED_BODY()
	
public:
	static UEclipseAssetManager& Get();

	// 모든 데이터 에셋이 쓰는 번들이름
	static const FName BundleGame;

	template <typename T>
	static T* GetAsset(const TSoftObjectPtr<T>& AssetPtr, bool bKeepOnMemory = false);

	template <typename T>
	static TSubclassOf<T> GetSubclass(const TSoftClassPtr<T>& AssetPtr, bool bKeepOnMemory = false);

	const UEclipseGameData* GetGameData();

protected:
	virtual void StartInitialLoading() override;

	static UObject* SynchronousLoadAsset(const FSoftObjectPath& AssetPath);
	static bool ShouldLogAssetLoad();

	void AddLoadedAsset(const UObject* Asset);

private:
	UPROPERTY(Transient)
	TObjectPtr<UEclipseGameData> GameData;

	// GetAsset(bKeepOnMemory=true), 게임 실행동안 유지되는 에셋
	UPROPERTY(Transient)
	TSet<TObjectPtr<const UObject>> LoadedAssets;

	FCriticalSection LoadedAssetsCritical;
};

template <typename T>
T* UEclipseAssetManager::GetAsset(const TSoftObjectPtr<T>& AssetPtr, bool bKeepOnMemory)
{
	T* LoadedAsset = nullptr;
	const FSoftObjectPath& AssetPath = AssetPtr.ToSoftObjectPath();
	if (AssetPath.IsValid())
	{
		LoadedAsset = AssetPtr.Get();
		if (!LoadedAsset)
		{
			LoadedAsset = Cast<T>(SynchronousLoadAsset(AssetPath));
			ensureAlwaysMsgf(LoadedAsset, TEXT("에셋을 불러오지 못했습니다.[%s]"), *AssetPtr.ToString());
		}

		if (LoadedAsset && bKeepOnMemory)
		{
			Get().AddLoadedAsset(Cast<UObject>(LoadedAsset));
		}
	}

	return LoadedAsset;
}

template <typename T>
TSubclassOf<T> UEclipseAssetManager::GetSubclass(const TSoftClassPtr<T>& AssetPtr, bool bKeepOnMemory)
{
	TSubclassOf<T> LoadedSubclass;
	const FSoftObjectPath& AssetPath = AssetPtr.ToSoftObjectPath();
	if (AssetPath.IsValid())
	{
		LoadedSubclass = AssetPtr.Get();
		if (!LoadedSubclass)
		{
			LoadedSubclass = Cast<UClass>(SynchronousLoadAsset(AssetPath));
			ensureAlwaysMsgf(LoadedSubclass, TEXT("에셋을 불러오지 못했습니다.[%s]"), *AssetPtr.ToString());
		}
	}

	if (LoadedSubclass && bKeepOnMemory)
	{
		Get().AddLoadedAsset(Cast<UObject>(LoadedSubclass));
	}

	return LoadedSubclass;

}