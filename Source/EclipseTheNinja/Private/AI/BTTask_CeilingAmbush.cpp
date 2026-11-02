#include "AI/BTTask_CeilingAmbush.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Character/CeilingEnemy.h"

UBTTask_CeilingAmbush::UBTTask_CeilingAmbush()
{
    NodeName = TEXT("Ceiling Ambush");
    INIT_TASK_NODE_NOTIFY_FLAGS();

    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_CeilingAmbush, TargetKey), AActor::StaticClass());
    TargetKey.SelectedKeyName = TEXT("TargetActor");
}

void UBTTask_CeilingAmbush::InitializeFromAsset(UBehaviorTree& Asset)
{
    Super::InitializeFromAsset(Asset);

    if (Asset.BlackboardAsset)
    {
        TargetKey.ResolveSelectedKey(*Asset.BlackboardAsset);
    }
}

EBTNodeResult::Type UBTTask_CeilingAmbush::CheckLanded(UBehaviorTreeComponent& OwnerComp) const
{
    const AAIController* AI = OwnerComp.GetAIOwner();
    const ACeilingEnemy* Enemy = AI
        ? Cast<ACeilingEnemy>(AI->GetPawn())
        : nullptr;

    if (!Enemy || Enemy->IsDead())
    {
        return EBTNodeResult::Failed;
    }

    return Enemy->GetCeilingPhase() == ECeilingEnemyPhase::Grounded
        ? EBTNodeResult::Succeeded
        : EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_CeilingAmbush::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AI = OwnerComp.GetAIOwner();
    ACeilingEnemy* Enemy = AI
        ? Cast<ACeilingEnemy>(AI->GetPawn())
        : nullptr;

    if (!Enemy || Enemy->IsDead())
    {
        return EBTNodeResult::Failed;
    }

    // 매달려 있으면 기록된 대상으로 기습을 시작한다. 대상이 사라졌으면 다시 기다리도록 실패한다.
    if (Enemy->GetCeilingPhase() == ECeilingEnemyPhase::Hanging)
    {
        const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
        APawn* Target = BB && TargetKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass()
            ? Cast<APawn>(BB->GetValueAsObject(TargetKey.SelectedKeyName))
            : nullptr;

        if (!Enemy->IsValidAITarget(Target))
        {
            return EBTNodeResult::Failed;
        }

        Enemy->StartAmbush(Target);
    }

    return CheckLanded(OwnerComp);
}

void UBTTask_CeilingAmbush::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    const EBTNodeResult::Type Result = CheckLanded(OwnerComp);
    if (Result == EBTNodeResult::InProgress)
    {
        return;
    }

    FinishLatentTask(OwnerComp, Result);
}
