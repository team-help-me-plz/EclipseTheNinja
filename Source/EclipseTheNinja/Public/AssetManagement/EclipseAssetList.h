
#pragma once

#include "CoreMinimal.h"
#include "EclipseAssetList.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class USoundBase;

/**
 * 미리 불러올 에셋 목록.
 * 모든 필드는 Client 번들 (라이라와 같은 이름). 데이터 에셋을 Client 번들로 로드하면 여기 있는 것도 함께 로드된다.
 */
USTRUCT(BlueprintType)
struct ECLIPSETHENINJA_API FEclipseAssetList
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, meta = (AssetBundles = "Game"))
	TArray<TSoftObjectPtr<UAnimMontage>> Montages;

	UPROPERTY(EditDefaultsOnly, meta = (AssetBundles = "Game"))
	TArray<TSoftObjectPtr<UNiagaraSystem>> Effects;

	UPROPERTY(EditDefaultsOnly, meta = (AssetBundles = "Game"))
	TArray<TSoftObjectPtr<USoundBase>> Sounds;

	// 머티리얼, 메시 등
	UPROPERTY(EditDefaultsOnly, meta = (AssetBundles = "Game"))
	TArray<TSoftObjectPtr<UObject>> Others;
};
