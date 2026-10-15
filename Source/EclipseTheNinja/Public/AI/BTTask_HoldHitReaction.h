#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_HoldHitReaction.generated.h"

UCLASS(Blueprintable)
class ECLIPSETHENINJA_API UBTTask_HoldHitReaction : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_HoldHitReaction();
protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
private:
    EBTNodeResult::Type CheckReaction(UBehaviorTreeComponent& OwnerComp) const;
};
