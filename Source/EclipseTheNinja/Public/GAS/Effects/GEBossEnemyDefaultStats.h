#pragma once

#include "CoreMinimal.h"
#include "GAS/Effects/GEDefaultStats.h"
#include "GEBossEnemyDefaultStats.generated.h"

/** 보스 적의 시작 스탯. 패턴 피해량은 각 패턴 어빌리티가 따로 가진다. */
UCLASS()
class ECLIPSETHENINJA_API UGEBossEnemyDefaultStats : public UGEDefaultStats
{
	GENERATED_BODY()

public:
	UGEBossEnemyDefaultStats();
};
