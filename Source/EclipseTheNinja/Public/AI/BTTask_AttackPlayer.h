#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_AttackPlayer.generated.h"

class AEnemy;

UCLASS(Blueprintable)
class ECLIPSETHENINJA_API UBTTask_AttackPlayer : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_AttackPlayer();
    virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetKey;
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector SightKey;
    UPROPERTY(EditAnywhere, Category = "Attack", meta = (ClampMin = "0.1", Units = "s"))
    float AttackTimeout = 15.0f;
    /** 태스크 시작 후 공격을 한 번도 시작하지 못한 채 기다릴 수 있는 최대 시간(초). 넘으면 실패로 끝내 BT가 분기를 다시 고르게 한다. */
    UPROPERTY(EditAnywhere, Category = "Attack", meta = (ClampMin = "0.1", Units = "s"))
    float AttackStartTimeout = 10.0f;
protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type Result) override;
private:
    EBTNodeResult::Type UpdateAttack(UBehaviorTreeComponent& OwnerComp);
    void Cleanup();

    UPROPERTY(Transient)
    TWeakObjectPtr<AEnemy> Enemy;
    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> StartedTarget;
    bool bAttackStarted = false;
    double AttackStartedAt = 0.0;
    double TaskStartedAt = 0.0;
};
