#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GABossPatternBase.h"
#include "GABossPatternAttack.generated.h"

/**
 * 보스 근접 패턴 하나(몽타주 1개 + 앞쪽 타격 판정)를 실행하는 어빌리티.
 * 패턴마다 이 클래스를 부모로 하는 BP를 만들고 몽타주, 피해량, 판정 범위, 타격 시점을 지정한다.
 * 타격 시점 규칙은 UGABossPatternBase를, 쿨타임은 DT_BossPatterns 행의 Cooldown을 따른다.
 * 한 번의 판정에서는 같은 대상에게 한 번만 피해를 준다. 다단히트 패턴은 판정 시점을 여러 개 둔다.
 */
UCLASS()
class ECLIPSETHENINJA_API UGABossPatternAttack : public UGABossPatternBase
{
	GENERATED_BODY()

protected:
	// 보스 앞쪽을 쓸어 전투 대상에게 피해를 준다.
	virtual void OnPatternHit() override;

	/** 판정 1회당 피해량. 기획 시트의 대미지 값을 그대로 넣는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Damage", meta = (ClampMin = "0.0"))
	float Damage = 30.0f;

	/** 보스 앞쪽으로 판정 구를 쓸어낼 거리(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Damage", meta = (ClampMin = "1.0", Units = "cm"))
	float AttackRange = 250.0f;

	/** 판정 구의 반지름(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Damage", meta = (ClampMin = "1.0", Units = "cm"))
	float AttackRadius = 80.0f;

	/** 디버그용 판정 범위 표시. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Debug")
	bool bDrawDebugHit = false;
};
