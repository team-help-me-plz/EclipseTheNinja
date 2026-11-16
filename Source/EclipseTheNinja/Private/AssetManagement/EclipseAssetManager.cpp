
#include "AssetManagement/EclipseAssetManager.h"

#include "AssetManagement/EclipseGameData.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeLock.h"
#include "Stats/StatsMisc.h"

const FName UEclipseAssetManager::BundleGame(TEXT("Game"));

UEclipseAssetManager& UEclipseAssetManager::Get()
{
	check(GEngine);
	if (UEclipseAssetManager* Singleton = Cast<UEclipseAssetManager>(GEngine->AssetManager))
	{
		return *Singleton;
	}

	UE_LOG(LogTemp, Fatal, TEXT("UEclipseAssetManager Casting Failure, DefaultEngine.ini 수정 필요"));

	return *NewObject<UEclipseAssetManager>();
}

const UEclipseGameData* UEclipseAssetManager::GetGameData()
{
	if (GameData)
	{
		return GameData;
	}

	// 경로를 따로 적지 않고, 스캔된 EclipseGameData 타입 에셋을 찾아 쓴다 (프로젝트에 하나만 둔다)
	TArray<FPrimaryAssetId> Ids;
	GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("EclipseGameData")), Ids);
	if (Ids.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[AssetManager] EclipseGameData 데이터 에셋을 찾지 못했습니다. /Game 아래에 하나 만들어 주세요."));
		return nullptr;
	}
	if (Ids.Num() > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AssetManager] EclipseGameData가 %d개 있습니다. 하나만 두세요. %s를 사용합니다."),
			Ids.Num(), *Ids[0].ToString());
	}

	const FPrimaryAssetId& Id = Ids[0];
	if (TSharedPtr<FStreamableHandle> Handle = LoadPrimaryAsset(Id, { BundleGame }))
	{
		Handle->WaitUntilComplete(.0f, false);
	}
	GameData = Cast<UEclipseGameData>(GetPrimaryAssetObject(Id));
	if (!GameData)
	{
		UE_LOG(LogTemp, Error, TEXT("[AssetManager] %s를 불러오지 못했습니다."), *Id.ToString());
	}

	return GameData;
}

void UEclipseAssetManager::StartInitialLoading()
{
	// Super가 ini의 PrimaryAssetTypesToScan을 스캔한다. 빠지면 Primary Asset이 하나도 등록되지 않는다
	Super::StartInitialLoading();

	// 경로 없이 타입으로 찾으므로, 에디터처럼 에셋 레지스트리 스캔이 비동기로 도는 경우 스캔이 끝난 뒤에 찾는다
	// (패키징 빌드는 스캔이 이미 끝나 있어 바로 호출된다)
	CallOrRegister_OnCompletedInitialScan(FSimpleMulticastDelegate::FDelegate::CreateWeakLambda(this, [this]()
	{
		GetGameData();
	}));
}

UObject* UEclipseAssetManager::SynchronousLoadAsset(const FSoftObjectPath & AssetPath)
{
	if(!AssetPath.IsValid())
		return nullptr;

	TUniquePtr<FScopeLogTime> LogTime;
	if (ShouldLogAssetLoad())
	{
		LogTime = MakeUnique<FScopeLogTime>(*FString::Printf(TEXT("동기 로드 [%s]"), *AssetPath.ToString()), nullptr, FScopeLogTime::ScopeLog_Seconds);
	}

	if (UAssetManager::IsInitialized())
	{
		return UAssetManager::GetStreamableManager().LoadSynchronous(AssetPath, false);
	}

	return AssetPath.TryLoad();
}

bool UEclipseAssetManager::ShouldLogAssetLoad()
{
	static const bool bLog = FParse::Param(FCommandLine::Get(), TEXT("LogAssetLoads"));
	return bLog;
}

void UEclipseAssetManager::AddLoadedAsset(const UObject* Asset)
{
	if (ensureAlways(Asset))
	{
		FScopeLock Lock(&LoadedAssetsCritical);
		LoadedAssets.Add(Asset);
	}
}
