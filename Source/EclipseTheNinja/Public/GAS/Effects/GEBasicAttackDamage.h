#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GEBasicAttackDamage.generated.h"

/** Writes attack damage to the meta attribute; StatAttributeSet subtracts health. */
UCLASS()
class ECLIPSETHENINJA_API UGEBasicAttackDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGEBasicAttackDamage();
};
