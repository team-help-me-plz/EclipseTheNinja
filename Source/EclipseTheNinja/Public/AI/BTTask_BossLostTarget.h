#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_BossLostTarget.generated.h"

class UBossLostTargetComponent;

/**
 * 보스가 전투 중 대상을 놓쳤을 때 실행하는 태스크. 마지막 목격 위치 접근 → 포효 → 난사 → 순찰 복귀를 UBossLostTargetComponent에 맡긴다.
 * 보스 BT의 Combat 분기에서 "대상은 없고 마지막 목격 위치는 있는" 자리(HasLastKnownLocation)에 둔다.
 *
 * - 난사가 끝나면 컴포넌트가 대상을 잊어 Blackboard가 일반 상태로 바뀌고, Decorator가 이 태스크를 끝낸다.
 * - 대상을 다시 보는 등으로 중단되면, 포효 중일 때는 포효가 끝날 때까지 기다렸다가 중단한다.
 */
UCLASS()
class ECLIPSETHENINJA_API UBTTask_BossLostTarget : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_BossLostTarget();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
    /** 실행 중인 보스의 놓침 처리 컴포넌트 */
    TWeakObjectPtr<UBossLostTargetComponent> LostTargetComp;

    /** 중단 요청을 받고 포효가 끝나기를 기다리는 중인지 */
    bool bAbortRequested = false;
};
