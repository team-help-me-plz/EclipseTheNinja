#include "GAS/Ability/AbilityAssetPreloader.h"
#include "Engine/AssetManager.h"

void FAbilityAssetPreloader::Request(const TArray<FSoftObjectPath>& InPaths, FStreamableDelegate OnLoaded)
{
	Cancel();

	TArray<FSoftObjectPath> Paths;
	for (const FSoftObjectPath& Path : InPaths)
	{
		if (!Path.IsNull()) Paths.AddUnique(Path);
	}
	if (Paths.IsEmpty()) return;

	this->Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(MoveTemp(Paths), MoveTemp(OnLoaded));
}

void FAbilityAssetPreloader::Cancel()
{
	if (this->Handle.IsValid())
	{
		this->Handle->CancelHandle();
		this->Handle.Reset();
	}
}
