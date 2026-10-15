#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ChasePlayer.generated.h"

UCLASS(Blueprintable)
class ECLIPSETHENINJA_API UBTTask_ChasePlayer : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_ChasePlayer();
    virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetKey;
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector SightKey;
protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type Result) override;
private:
    EBTNodeResult::Type UpdateChase(UBehaviorTreeComponent& OwnerComp, float DeltaSeconds) const;
    void StopMovement(UBehaviorTreeComponent& OwnerComp) const;
};
