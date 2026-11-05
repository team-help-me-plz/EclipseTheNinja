#pragma once

#include "CoreMinimal.h"
#include "Character/Enemy.h"
#include "TorchEnemy.generated.h"

class UAllySummonComponent;
class UNiagaraComponent;
class UStaticMeshComponent;

/**
 * 횃불을 들고 다니다 플레이어를 발견하면 지원을 부르는 근접 적.
 *
 * 전투에 처음 들어갈 때 한 번 소리쳐 동료를 부르고(UAllySummonComponent, GA_SummonAllies),
 * 그 뒤에는 기본 근접 적처럼 추적·공격한다. 행동 순서는 BT_TorchEnemy가 정한다.
 * Wants Summon Allies 분기에서 Summon Allies를 실행하고, 그 외에는 Run Behavior(BT_Enemy)로 기본 근접 적과 같은 트리를 쓴다.
 * 횃불은 손 소켓에 붙인 메시와 불 이펙트 컴포넌트다. 소켓과 위치는 BP에서 맞춘다.
 */
UCLASS()
class ECLIPSETHENINJA_API ATorchEnemy : public AEnemy
{
	GENERATED_BODY()

public:
	ATorchEnemy();

	// 체크포인트 리스폰 1단계: 기본 초기화 후 소환 기회를 되돌린다. 소환된 동료는 체크포인트가 풀에 돌려보낸다.
	virtual void ResetToInitial_Implementation() override;

	/** 지원 소환 컴포넌트 */
	UAllySummonComponent* GetAllySummonComponent() const { return AllySummonComp; }

protected:
	/** 전투에 들어가면 한 번 동료를 부른다. 소환할 적·수·몽타주는 이 컴포넌트에서 정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Summon")
	TObjectPtr<UAllySummonComponent> AllySummonComp;

	/** 손에 든 횃불 메시 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Torch")
	TObjectPtr<UStaticMeshComponent> TorchMesh;

	/** 횃불 끝의 불 이펙트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Torch")
	TObjectPtr<UNiagaraComponent> TorchFlame;
};
