#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SummonAllies.generated.h"

/**
 * GA_SummonAllies를 실행하고 소리치는 동작이 끝날 때까지 기다린다.
 * 동료를 불렀으면 성공, 부르기 전에 끊겼으면 실패한다. 실패해도 컴포넌트가 계속 소환을 원하면 Wants Summon Allies가 다시 이 분기를 고른다.
 * 소환 판단과 위치 계산은 UAllySummonComponent가, 몽타주와 소환 시점은 GA가 맡는다.
 */
UCLASS(Blueprintable)
class ECLIPSETHENINJA_API UBTTask_SummonAllies : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_SummonAllies();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
    // 소환 동작이 진행 중이면 InProgress, 끝났으면 소환 여부에 따라 성공/실패를 돌려준다.
    EBTNodeResult::Type CheckSummon(UBehaviorTreeComponent& OwnerComp) const;
};
