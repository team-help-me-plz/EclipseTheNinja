#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GABossPatternBase.h"
#include "GABossPatternAttack.generated.h"

class AEnemy;
class UAbilitySystemComponent;

/**
 * 보스 근접 패턴 하나(몽타주 1개 + 타격마다 판정)를 실행하는 어빌리티.
 * 패턴마다 이 클래스를 부모로 하는 BP를 만들고 몽타주와 피해량을 지정한다.
 *
 * 몽타주의 Boss Attack Hit 노티파이마다 그 노티파이에 지정한 상자 영역을 그 순간 검사한다.
 * 타격마다 범위와 피해 배율을 따로 줄 수 있다. 쿨타임은 DT_BossPatterns 행의 Cooldown을 따른다.
 * 한 번의 판정에서는 같은 대상에게 한 번만 피해를 준다. 다단히트 패턴은 노티파이를 여러 개 둔다.
 */
UCLASS()
class ECLIPSETHENINJA_API UGABossPatternAttack : public UGABossPatternBase
{
	GENERATED_BODY()

protected:
	// 판정 영역 안의 전투 대상에게 피해를 준다.
	virtual void OnPatternHit(const FBossAttackHitArea& HitArea) override;

	// 대상에게 HitDamage만큼 피해를 준다. 대상이 이미 쓰러졌거나 피해를 받을 수 없으면 false.
	bool ApplyHitDamage(
		UAbilitySystemComponent* SourceASC,
		AActor* Target,
		const FHitResult& Hit,
		float HitDamage
	) const;

	// 액터 스케일을 곱한 판정 상자의 절반 크기. Offset이 액터 스케일을 따르므로 크기도 같이 맞춘다.
	static FVector GetScaledHitExtent(const FTransform& ActorTransform, const FBossAttackHitArea& HitArea);

	// 노티파이가 지정한 상자 영역에 겹친 Pawn을 모은다.
	void QueryHitArea(
		const AEnemy* Enemy,
		const FBossAttackHitArea& HitArea,
		TArray<FHitResult>& OutHits
	) const;

	// 보스 BP의 bDrawDebugAttack(SetDrawDebugAttack)이 켜져 있으면 판정 영역과 걸린 지점을 그린다.
	// 전투 대상이 영역 안에 있으면 초록, 없으면 빨강으로 그린다.
	void DrawDebugHitArea(
		const AEnemy* Enemy,
		const FBossAttackHitArea& HitArea,
		const TArray<FHitResult>& Hits,
		bool bTargetInArea
	) const;

	/** 판정 1회당 피해량. Boss Attack Hit 노티파이의 DamageScale이 곱해진다. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Damage", meta = (ClampMin = "0.0"))
	float Damage = 30.0f;
};
