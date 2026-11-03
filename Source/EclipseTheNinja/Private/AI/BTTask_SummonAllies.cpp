#include "AI/BTTask_SummonAllies.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Character/Enemy.h"
#include "Component/AllySummonComponent.h"
#include "GameplayAbilitySpec.h"
#include "GAS/AbilityInfo.h"

namespace
{
    // BT를 실행하는 적과 그 소환 컴포넌트를 찾는다.
    AEnemy* GetSummoner(const UBehaviorTreeComponent& OwnerComp, UAllySummonComponent*& OutSummonComp)
    {
        const AAIController* AI = OwnerComp.GetAIOwner();
        AEnemy* Enemy = AI
            ? Cast<AEnemy>(AI->GetPawn())
            : nullptr;

        OutSummonComp = Enemy
            ? Enemy->FindComponentByClass<UAllySummonComponent>()
            : nullptr;

        return Enemy;
    }
}

UBTTask_SummonAllies::UBTTask_SummonAllies()
{
    NodeName = TEXT("Summon Allies");
    INIT_TASK_NODE_NOTIFY_FLAGS();
}

EBTNodeResult::Type UBTTask_SummonAllies::CheckSummon(UBehaviorTreeComponent& OwnerComp) const
{
    UAllySummonComponent* SummonComp = nullptr;
    const AEnemy* Enemy = GetSummoner(OwnerComp, SummonComp);

    if (!Enemy || Enemy->IsDead() || !SummonComp)
    {
        return EBTNodeResult::Failed;
    }

    if (SummonComp->IsSummonInProgress())
    {
        return EBTNodeResult::InProgress;
    }

    return SummonComp->HasSummoned()
        ? EBTNodeResult::Succeeded
        : EBTNodeResult::Failed;
}

EBTNodeResult::Type UBTTask_SummonAllies::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UAllySummonComponent* SummonComp = nullptr;
    AEnemy* Enemy = GetSummoner(OwnerComp, SummonComp);

    if (!Enemy || Enemy->IsDead() || !SummonComp || !SummonComp->WantsToSummon())
    {
        return EBTNodeResult::Failed;
    }

    // 소환 어빌리티는 AbilityClassArray에 SummonAllies 입력 ID로 부여되어 있어야 한다.
    UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
    const FGameplayAbilitySpec* Spec = Enemy->FindAbilitySpecByInputID(EAbilityInputID::SummonAllies);

    if (!SummonComp->IsSummonInProgress())
    {
        if (!ASC || !Spec || !ASC->TryActivateAbility(Spec->Handle))
        {
            return EBTNodeResult::Failed;
        }
    }

    return CheckSummon(OwnerComp);
}

void UBTTask_SummonAllies::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    const EBTNodeResult::Type Result = CheckSummon(OwnerComp);
    if (Result == EBTNodeResult::InProgress)
    {
        return;
    }

    FinishLatentTask(OwnerComp, Result);
}

EBTNodeResult::Type UBTTask_SummonAllies::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // 분기가 끊기면 소리치는 동작도 멈춘다.
    UAllySummonComponent* SummonComp = nullptr;
    AEnemy* Enemy = GetSummoner(OwnerComp, SummonComp);

    UAbilitySystemComponent* ASC = Enemy
        ? Enemy->GetAbilitySystemComponent()
        : nullptr;

    const FGameplayAbilitySpec* Spec = Enemy
        ? Enemy->FindAbilitySpecByInputID(EAbilityInputID::SummonAllies)
        : nullptr;

    if (ASC && Spec && Spec->IsActive())
    {
        ASC->CancelAbilityHandle(Spec->Handle);
    }

    return EBTNodeResult::Aborted;
}
