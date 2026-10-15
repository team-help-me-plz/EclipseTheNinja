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
};
