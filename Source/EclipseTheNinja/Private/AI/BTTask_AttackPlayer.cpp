#include "AI/BTTask_AttackPlayer.h"

#include "Character/Enemy.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "Engine/World.h"

UBTTask_AttackPlayer::UBTTask_AttackPlayer()
{
    NodeName = TEXT("Attack Player");

    // 공격 시작 여부와 타깃, 시작 시간은 적마다 별도로 저장한다.
    bCreateNodeInstance = true;
    INIT_TASK_NODE_NOTIFY_FLAGS();

    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_AttackPlayer, TargetKey), AActor::StaticClass());
    SightKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_AttackPlayer, SightKey));

    TargetKey.SelectedKeyName = TEXT("TargetActor");
    SightKey.SelectedKeyName = TEXT("bHasLineofSight");
}

void UBTTask_AttackPlayer::InitializeFromAsset(UBehaviorTree& Asset)
{
    Super::InitializeFromAsset(Asset);
    if (!Asset.BlackboardAsset) return;

    TargetKey.ResolveSelectedKey(*Asset.BlackboardAsset);
    SightKey.ResolveSelectedKey(*Asset.BlackboardAsset);
}

EBTNodeResult::Type UBTTask_AttackPlayer::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    // 이전 실행에서 사용한 적과 공격 상태를 초기화한다.
    Enemy.Reset();
    bAttackStarted = false;
    StartedTarget.Reset();
    AttackStartedAt = 0.0;

    AAIController* AI = OwnerComp.GetAIOwner();
    if (!AI) return EBTNodeResult::Failed;

    Enemy = Cast<AEnemy>(AI->GetPawn());

    return UpdateAttack(OwnerComp);
}

EBTNodeResult::Type UBTTask_AttackPlayer::UpdateAttack(UBehaviorTreeComponent& OwnerComp)
{
    AEnemy* Pawn = Enemy.Get();
    if (!Pawn) return EBTNodeResult::Failed;
    if (Pawn->IsDead()) return EBTNodeResult::Failed;

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return EBTNodeResult::Failed;

    if (TargetKey.SelectedKeyType != UBlackboardKeyType_Object::StaticClass()) return EBTNodeResult::Failed;
    if (SightKey.SelectedKeyType != UBlackboardKeyType_Bool::StaticClass()) return EBTNodeResult::Failed;

    if (!Pawn->IsReadyForAI()) return EBTNodeResult::InProgress;

    // 시야를 잃거나 공격 거리에서 벗어나거나 피격되면 공격 태스크를 끝낸다.
    AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
    if (!BB->GetValueAsBool(SightKey.SelectedKeyName)) return EBTNodeResult::Failed;
    if (!Pawn->IsWithinAttackRange(Target)) return EBTNodeResult::Failed;
    if (Pawn->IsPlayingHitReaction()) return EBTNodeResult::Failed;

    Pawn->StopHorizontalMovement();
    const double Now = Pawn->GetWorld()->GetTimeSeconds();

    // 이미 시작한 공격은 완료될 때까지 기다린다.
    if (bAttackStarted)
    {
        if (StartedTarget.Get() != Target) return EBTNodeResult::Failed;

        const double AttackElapsed = Now - AttackStartedAt;
        if (AttackElapsed >= AttackTimeout) return EBTNodeResult::Failed;
        if (Pawn->IsAttackActive()) return EBTNodeResult::InProgress;

        return EBTNodeResult::Succeeded;
    }

    // 실제 공격 간격과 어빌리티 실행 가능 여부는 Enemy가 검사한다.
    if (Pawn->TryStartAIBaseAttack(Target))
    {
        bAttackStarted = true;
        StartedTarget = Target;
        AttackStartedAt = Now;
    }

    // 아직 공격할 수 없으면 다음 Tick에서 다시 시도한다.
    return EBTNodeResult::InProgress;
}

void UBTTask_AttackPlayer::TickTask(UBehaviorTreeComponent& OwnerComp, uint8*, float)
{
    const EBTNodeResult::Type Result = UpdateAttack(OwnerComp);
    if (Result == EBTNodeResult::InProgress) return;

    FinishLatentTask(OwnerComp, Result);
}

void UBTTask_AttackPlayer::Cleanup()
{
    AEnemy* Pawn = Enemy.Get();
    if (Pawn)
    {
        Pawn->StopHorizontalMovement();

        // 이 태스크가 시작한 공격만 취소한다.
        if (bAttackStarted)
        {
            Pawn->CancelAIBaseAttack();
        }
    }

    bAttackStarted = false;
    StartedTarget.Reset();
    Enemy.Reset();
}

EBTNodeResult::Type UBTTask_AttackPlayer::AbortTask(UBehaviorTreeComponent&, uint8*)
{
    Cleanup();
    return EBTNodeResult::Aborted;
}

void UBTTask_AttackPlayer::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* Memory, EBTNodeResult::Type Result)
{
    Cleanup();
    Super::OnTaskFinished(OwnerComp, Memory, Result);
}
