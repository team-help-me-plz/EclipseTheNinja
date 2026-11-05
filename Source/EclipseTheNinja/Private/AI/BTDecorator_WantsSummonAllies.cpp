#include "AI/BTDecorator_WantsSummonAllies.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Component/AllySummonComponent.h"

UBTDecorator_WantsSummonAllies::UBTDecorator_WantsSummonAllies()
{
    NodeName = TEXT("Wants Summon Allies");
    INIT_DECORATOR_NODE_NOTIFY_FLAGS();

    // 소환 분기를 끝내는 건 태스크가 스스로 하므로, 더 낮은 분기를 끊는 중단만 쓴다.
    FlowAbortMode = EBTFlowAbortMode::LowerPriority;
    bAllowAbortNone = true;
    bAllowAbortLowerPri = true;
    bAllowAbortChildNodes = false;
}

bool UBTDecorator_WantsSummonAllies::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
    const AAIController* AI = OwnerComp.GetAIOwner();
    const APawn* Pawn = AI
        ? AI->GetPawn()
        : nullptr;

    const UAllySummonComponent* SummonComp = Pawn
        ? Pawn->FindComponentByClass<UAllySummonComponent>()
        : nullptr;

    return SummonComp && SummonComp->WantsToSummon();
}

void UBTDecorator_WantsSummonAllies::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    FWantsSummonAlliesMemory* Memory = CastInstanceNodeMemory<FWantsSummonAlliesMemory>(NodeMemory);
    Memory->bLastResult = CalculateRawConditionValue(OwnerComp, NodeMemory);
}

void UBTDecorator_WantsSummonAllies::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    FWantsSummonAlliesMemory* Memory = CastInstanceNodeMemory<FWantsSummonAlliesMemory>(NodeMemory);

    // 값이 바뀔 때만 분기를 다시 평가한다.
    const bool bResult = CalculateRawConditionValue(OwnerComp, NodeMemory);
    if (bResult == Memory->bLastResult)
    {
        return;
    }

    Memory->bLastResult = bResult;
    OwnerComp.RequestBranchEvaluation(*this);
}

uint16 UBTDecorator_WantsSummonAllies::GetInstanceMemorySize() const
{
    return sizeof(FWantsSummonAlliesMemory);
}
