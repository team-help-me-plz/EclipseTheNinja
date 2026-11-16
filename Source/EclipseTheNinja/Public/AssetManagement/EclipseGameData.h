

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AssetManagement/EclipseAssetList.h"
#include "EclipseGameData.generated.h"


/**
 * 게임 내내 쓰는 전역 데이터
 * 엔진 시작 시 동기 로드되고 해제되지 않는다. 프로젝트에 하나만 둔다.
 */

UCLASS(BlueprintType, Const)
class ECLIPSETHENINJA_API UEclipseGameData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, Category = "Preload")
	FEclipseAssetList GlobalAssets;

};
