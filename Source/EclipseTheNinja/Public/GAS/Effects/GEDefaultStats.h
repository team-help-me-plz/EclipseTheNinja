#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GEDefaultStats.generated.h"

/** 시작 시 스탯을 지정한 값으로 덮어쓰는 Instant GE의 부모 클래스. BP 자식에서 Modifiers 값을 바꿔 조정할 수 있다. */
UCLASS(Abstract)
class ECLIPSETHENINJA_API UGEDefaultStats : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGEDefaultStats();

protected:
	// Health는 MaxHealth로 Clamp되므로 MaxHealth를 먼저 덮어쓴다.
	void SetDefaultStats(
		float MaxHealth,
		float Health,
		float AttackPower
	);

private:
	// 지정한 Attribute를 Override하는 Modifier를 추가한다.
	void AddOverrideModifier(
		const FGameplayAttribute& Attribute,
		float Value
	);
};

/** 플레이어 캐릭터의 시작 스탯. */
UCLASS()
class ECLIPSETHENINJA_API UGEPlayerDefaultStats : public UGEDefaultStats
{
	GENERATED_BODY()

public:
	UGEPlayerDefaultStats();
};

/** 근거리 적의 시작 스탯. */
UCLASS()
class ECLIPSETHENINJA_API UGEMeleeEnemyDefaultStats : public UGEDefaultStats
{
	GENERATED_BODY()

public:
	UGEMeleeEnemyDefaultStats();
};

/** 원거리(표창) 적의 시작 스탯. */
UCLASS()
class ECLIPSETHENINJA_API UGERangedEnemyDefaultStats : public UGEDefaultStats
{
	GENERATED_BODY()

public:
	UGERangedEnemyDefaultStats();
};

/** 보스 적의 시작 스탯. 패턴 피해량은 각 패턴 어빌리티가 따로 가진다. */
UCLASS()
class ECLIPSETHENINJA_API UGEBossEnemyDefaultStats : public UGEDefaultStats
{
	GENERATED_BODY()

public:
	UGEBossEnemyDefaultStats();
};
