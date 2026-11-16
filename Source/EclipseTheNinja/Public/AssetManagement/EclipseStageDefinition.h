

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AssetManagement/EclipseAssetList.h"
#include "EclipseStageDefinition.generated.h"

class UEclipseAssetSet;

/**
 * 맵 하나에 필요한 것
 * 맵의 World Settings → StageDefinition에서 고른다.
 */
UCLASS()
class ECLIPSETHENINJA_API UEclipseStageDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, Category = "Preload")
	FEclipseAssetList Assets;

	UPROPERTY(EditDefaultsOnly, Category = "Preload")
	TArray<TObjectPtr<UEclipseAssetSet>> AssetSets;
};
