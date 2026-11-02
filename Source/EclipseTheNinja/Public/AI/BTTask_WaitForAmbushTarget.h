#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_WaitForAmbushTarget.generated.h"

/**
 * 천장에 매달린 동안 아래 감지 구역에 대상이 들어올 때까지 기다린다.
 * 대상을 찾으면 TargetKey에 기록하고 성공한다. 매달린 상태가 아니면(맞아서 떨어지는 중 등) 바로 성공해
 * 다음 기습 태스크가 착지까지 기다리게 한다.
 */
UCLASS(Blueprintable)
class ECLIPSETHENINJA_API UBTTask_WaitForAmbushTarget : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_WaitForAmbushTarget();
    virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

    /** 찾은 기습 대상을 기록할 키 */
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetKey;

    /** 감지 구역 검사 간격(초) */
    UPROPERTY(EditAnywhere, Category = "Ambush", meta = (ClampMin = "0.02", Units = "s"))
    float CheckInterval = 0.1f;

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual uint16 GetInstanceMemorySize() const override;

private:
    // 적마다 따로 쓰는 검사 경과 시간
    struct FWaitForAmbushTargetMemory
    {
        float Elapsed = 0.0f;
    };

    // 감지 구역을 검사해 대상을 찾으면 성공, 계속 기다릴 때는 InProgress를 돌려준다.
    EBTNodeResult::Type CheckTarget(UBehaviorTreeComponent& OwnerComp) const;
};
