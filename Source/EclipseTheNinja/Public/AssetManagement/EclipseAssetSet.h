

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AssetManagement/EclipseAssetList.h"
#include "EclipseAssetSet.generated.h"

 /**
  * 여러 스테이지가 같이 쓰는 묶음 (라이라 ULyraExperienceActionSet).
  * 예: 성 안 공용 적·함정 이펙트
  */
UCLASS(BlueprintType, Const)
class ECLIPSETHENINJA_API UEclipseAssetSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Preload")
	FEclipseAssetList Assetes;
};
