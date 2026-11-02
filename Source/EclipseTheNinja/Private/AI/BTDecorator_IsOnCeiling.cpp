#include "AI/BTDecorator_IsOnCeiling.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Character/CeilingEnemy.h"

UBTDecorator_IsOnCeiling::UBTDecorator_IsOnCeiling()
{
    NodeName = TEXT("Is On Ceiling");

    // 천장 단계가 끝나면 진행 중인 태스크가 스스로 끝나므로 중단 요청은 쓰지 않는다.
    bAllowAbortNone = true;
    bAllowAbortLowerPri = false;
    bAllowAbortChildNodes = false;
}

bool UBTDecorator_IsOnCeiling::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
    const AAIController* AI = OwnerComp.GetAIOwner();
    const ACeilingEnemy* Enemy = AI
        ? Cast<ACeilingEnemy>(AI->GetPawn())
        : nullptr;

    if (!Enemy || Enemy->IsDead())
    {
        return false;
    }

    return Enemy->GetCeilingPhase() != ECeilingEnemyPhase::Grounded;
}
