#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_CeilingAmbush.generated.h"

/**
 * 천장 매복 적의 기습을 시작하고, 착지해 일반 근접 적이 될 때까지 기다린다.
 * 매달려 있으면 TargetKey 대상으로 기습을 시작한다. 맞아서 공격 없이 떨어지는 중이면 착지만 기다린다.
 * 하강·타격·착지 처리는 ACeilingEnemy와 GA_CeilingAmbush가 맡는다.
 */
UCLASS(Blueprintable)
class ECLIPSETHENINJA_API UBTTask_CeilingAmbush : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_CeilingAmbush();
    virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

    /** 기습할 대상 키. Wait For Ambush Target이 기록한 키와 같아야 한다. */
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetKey;

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
    // 착지해 천장 단계가 끝났으면 성공, 아직이면 InProgress를 돌려준다.
    EBTNodeResult::Type CheckLanded(UBehaviorTreeComponent& OwnerComp) const;
};
