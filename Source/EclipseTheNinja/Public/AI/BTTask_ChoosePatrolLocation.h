#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ChoosePatrolLocation.generated.h"

UCLASS(Blueprintable)
class ECLIPSETHENINJA_API UBTTask_ChoosePatrolLocation : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_ChoosePatrolLocation();
    virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector PatrolLocationKey;

protected:
    // Task 시작할 때 1회 호출 InProgress를 반환했을 때는 태스크가 계속 실행 상태로 남아 TickTask()에서 다시 시도
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    // Task가 진행중일 때 매 프레임마다 반복
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
private:
    EBTNodeResult::Type SelectDestination(UBehaviorTreeComponent& OwnerComp) const;
};
