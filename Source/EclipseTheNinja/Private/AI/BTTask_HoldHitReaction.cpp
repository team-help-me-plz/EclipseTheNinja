#include "AI/BTTask_HoldHitReaction.h"

#include "Character/Enemy.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UBTTask_HoldHitReaction::UBTTask_HoldHitReaction()
{
    NodeName = TEXT("Hold During Hit Reaction");
    INIT_TASK_NODE_NOTIFY_FLAGS();
}

EBTNodeResult::Type UBTTask_HoldHitReaction::CheckReaction(UBehaviorTreeComponent& OwnerComp) const
{
    AAIController* AI = OwnerComp.GetAIOwner();
    if (!AI) return EBTNodeResult::Failed;

    AEnemy* Enemy = Cast<AEnemy>(AI->GetPawn());
    if (!Enemy) return EBTNodeResult::Failed;
    if (Enemy->IsDead()) return EBTNodeResult::Failed;
    if (!Enemy->IsReadyForAI()) return EBTNodeResult::InProgress;

    // 중력에 의한 낙하는 유지하고 수평 이동만 멈춘다.
    Enemy->StopHorizontalMovement();

    // 피격 몽타주가 끝나면 BT의 다음 행동을 허용한다.
    if (Enemy->IsPlayingHitReaction()) return EBTNodeResult::InProgress;

    return EBTNodeResult::Succeeded;
}

EBTNodeResult::Type UBTTask_HoldHitReaction::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    return CheckReaction(OwnerComp);
}

void UBTTask_HoldHitReaction::TickTask(UBehaviorTreeComponent& OwnerComp, uint8*, float)
{
    const EBTNodeResult::Type Result = CheckReaction(OwnerComp);
    if (Result == EBTNodeResult::InProgress) return;

    FinishLatentTask(OwnerComp, Result);
}

EBTNodeResult::Type UBTTask_HoldHitReaction::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    // 적이 없어도 태스크 자체는 정상적으로 중단한다.
    AAIController* AI = OwnerComp.GetAIOwner();
    if (!AI) return EBTNodeResult::Aborted;

    AEnemy* Enemy = Cast<AEnemy>(AI->GetPawn());
    if (!Enemy) return EBTNodeResult::Aborted;

    Enemy->StopHorizontalMovement();

    return EBTNodeResult::Aborted;
}
