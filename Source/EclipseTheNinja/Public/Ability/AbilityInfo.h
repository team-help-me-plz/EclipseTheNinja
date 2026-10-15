#pragma once

#include "CoreMinimal.h"
#include "AbilityInfo.generated.h"

class UGameplayAbility;

/** Designer-assigned startup ability and its GAS input identifier. */
USTRUCT(BlueprintType)
struct FAbilityInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 AbilityLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 InputID = INDEX_NONE;
};
