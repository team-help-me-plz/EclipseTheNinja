#include "AI/BTTask_WaitForAmbushTarget.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Character/CeilingEnemy.h"

UBTTask_WaitForAmbushTarget::UBTTask_WaitForAmbushTarget()
{
    NodeName = TEXT("Wait For Ambush Target");
    INIT_TASK_NODE_NOTIFY_FLAGS();

    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_WaitForAmbushTarget, TargetKey), AActor::StaticClass());
    TargetKey.SelectedKeyName = TEXT("TargetActor");
}

void UBTTask_WaitForAmbushTarget::InitializeFromAsset(UBehaviorTree& Asset)
{
    Super::InitializeFromAsset(Asset);

    if (Asset.BlackboardAsset)
    {
        TargetKey.ResolveSelectedKey(*Asset.BlackboardAsset);
    }
}

uint16 UBTTask_WaitForAmbushTarget::GetInstanceMemorySize() const
{
    return sizeof(FWaitForAmbushTargetMemory);
}

EBTNodeResult::Type UBTTask_WaitForAmbushTarget::CheckTarget(UBehaviorTreeComponent& OwnerComp) const
{
    AAIController* AI = OwnerComp.GetAIOwner();
    ACeilingEnemy* Enemy = AI
        ? Cast<ACeilingEnemy>(AI->GetPawn())
        : nullptr;

    if (!Enemy || Enemy->IsDead())
    {
        return EBTNodeResult::Failed;
    }

    if (!Enemy->IsReadyForAI())
    {
        return EBTNodeResult::InProgress;
    }

    // 이미 매달린 상태가 아니면 기다릴 필요가 없다. 다음 기습 태스크가 착지까지 맡는다.
    if (Enemy->GetCeilingPhase() != ECeilingEnemyPhase::Hanging)
    {
        return EBTNodeResult::Succeeded;
    }

    APawn* Target = Enemy->FindAmbushTarget(CheckInterval);
    if (!Target)
    {
        return EBTNodeResult::InProgress;
    }

    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
    {
        if (TargetKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass())
        {
            BB->SetValueAsObject(TargetKey.SelectedKeyName, Target);
        }
    }

    return EBTNodeResult::Succeeded;
}

EBTNodeResult::Type UBTTask_WaitForAmbushTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    CastInstanceNodeMemory<FWaitForAmbushTargetMemory>(NodeMemory)->Elapsed = 0.0f;
    return CheckTarget(OwnerComp);
}

void UBTTask_WaitForAmbushTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    // 감지 구역 검사는 일정 간격으로만 한다.
    FWaitForAmbushTargetMemory* Memory = CastInstanceNodeMemory<FWaitForAmbushTargetMemory>(NodeMemory);
    Memory->Elapsed += DeltaSeconds;

    if (Memory->Elapsed < CheckInterval)
    {
        return;
    }

    Memory->Elapsed = 0.0f;

    const EBTNodeResult::Type Result = CheckTarget(OwnerComp);
    if (Result == EBTNodeResult::InProgress)
    {
        return;
    }

    FinishLatentTask(OwnerComp, Result);
}
