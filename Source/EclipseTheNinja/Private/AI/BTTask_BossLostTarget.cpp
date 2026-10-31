#include "AI/BTTask_BossLostTarget.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Component/BossLostTargetComponent.h"

UBTTask_BossLostTarget::UBTTask_BossLostTarget()
{
    NodeName = TEXT("Boss Lost Target");

    // 컴포넌트와 중단 대기 여부를 멤버로 저장하므로 보스마다 따로 생성한다.
    bCreateNodeInstance = true;
    INIT_TASK_NODE_NOTIFY_FLAGS();
}

EBTNodeResult::Type UBTTask_BossLostTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    bAbortRequested = false;

    const AAIController* AI = OwnerComp.GetAIOwner();
    const APawn* Pawn = AI ? AI->GetPawn() : nullptr;
    LostTargetComp = Pawn ? Pawn->FindComponentByClass<UBossLostTargetComponent>() : nullptr;
    if (!LostTargetComp.IsValid())
    {
        return EBTNodeResult::Failed;
    }

    if (!LostTargetComp->BeginLostTarget())
    {
        return EBTNodeResult::Failed;
    }

    return EBTNodeResult::InProgress;
}

void UBTTask_BossLostTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8*, float DeltaSeconds)
{
    UBossLostTargetComponent* Comp = LostTargetComp.Get();
    if (!Comp)
    {
        if (bAbortRequested)
        {
            FinishLatentAbort(OwnerComp);
        }
        else
        {
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        }

        return;
    }

    // 중단 요청을 받았으면 포효가 끝나는 순간 놓침 처리를 멈추고 중단을 마친다.
    if (bAbortRequested)
    {
        if (Comp->IsRoaring())
        {
            return;
        }

        Comp->StopLostTarget();
        FinishLatentAbort(OwnerComp);
        return;
    }

    if (!Comp->UpdateLostTarget(DeltaSeconds))
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}

EBTNodeResult::Type UBTTask_BossLostTarget::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    UBossLostTargetComponent* Comp = LostTargetComp.Get();

    // 포효 중에는 대상을 다시 봐도 끝까지 재생한다. 끝날 때까지 TickTask에서 기다린다.
    if (Comp && Comp->IsRoaring())
    {
        bAbortRequested = true;
        return EBTNodeResult::InProgress;
    }

    if (Comp)
    {
        Comp->StopLostTarget();
    }

    return EBTNodeResult::Aborted;
}
