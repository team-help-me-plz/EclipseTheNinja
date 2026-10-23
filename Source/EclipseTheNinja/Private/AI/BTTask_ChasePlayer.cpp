#include "AI/BTTask_ChasePlayer.h"

#include "Character/Enemy.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"

UBTTask_ChasePlayer::UBTTask_ChasePlayer()
{
    NodeName = TEXT("Chase Player");
    INIT_TASK_NODE_NOTIFY_FLAGS();

    // 추적할 액터와 현재 시야 확보 여부를 Blackboard에서 읽는다.
    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_ChasePlayer, TargetKey), AActor::StaticClass());
    SightKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_ChasePlayer, SightKey));

    TargetKey.SelectedKeyName = TEXT("TargetActor");
    SightKey.SelectedKeyName = TEXT("bHasLineofSight");
}

void UBTTask_ChasePlayer::InitializeFromAsset(UBehaviorTree& Asset)
{
    Super::InitializeFromAsset(Asset);
    if (!Asset.BlackboardAsset) return;

    TargetKey.ResolveSelectedKey(*Asset.BlackboardAsset);
    SightKey.ResolveSelectedKey(*Asset.BlackboardAsset);
}

EBTNodeResult::Type UBTTask_ChasePlayer::UpdateChase(UBehaviorTreeComponent& OwnerComp, float DeltaSeconds) const
{
    AAIController* AI = OwnerComp.GetAIOwner();
    if (!AI) return EBTNodeResult::Failed;

    AEnemy* Enemy = Cast<AEnemy>(AI->GetPawn());
    if (!Enemy) return EBTNodeResult::Failed;
    if (Enemy->IsDead()) return EBTNodeResult::Failed;

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return EBTNodeResult::Failed;

    if (TargetKey.SelectedKeyType != UBlackboardKeyType_Object::StaticClass()) return EBTNodeResult::Failed;
    if (SightKey.SelectedKeyType != UBlackboardKeyType_Bool::StaticClass()) return EBTNodeResult::Failed;

    if (!Enemy->IsReadyForAI()) return EBTNodeResult::InProgress;

    // 시야를 잃으면 현재 위치 추적을 끝내고, BT의 다른 분기에서 마지막 관측 위치를 확인한다.
    AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
    if (!BB->GetValueAsBool(SightKey.SelectedKeyName)) return EBTNodeResult::Failed;
    if (!Enemy->IsValidAITarget(Target)) return EBTNodeResult::Failed;

    Enemy->SetBehaviorState(EEnemyBehaviorState::Chase);

    // 공격 전환 시점과 접근/후퇴 방식은 적의 전투 정책을 따른다.
    // 원거리 적은 쿨타임 동안 이 태스크에서 거리를 조절하고, 준비되면 BT가 공격 분기를 다시 선택하도록 종료한다.
    if (Enemy->CanStartAIAttack(Target)) return EBTNodeResult::Succeeded;
    Enemy->UpdateCombatMovement(Target, DeltaSeconds);

    return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_ChasePlayer::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    return UpdateChase(OwnerComp, 0.0f);
}

void UBTTask_ChasePlayer::TickTask(UBehaviorTreeComponent& OwnerComp, uint8*, float DeltaSeconds)
{
    const EBTNodeResult::Type Result = UpdateChase(OwnerComp, DeltaSeconds);
    if (Result == EBTNodeResult::InProgress) return;

    FinishLatentTask(OwnerComp, Result);
}

void UBTTask_ChasePlayer::StopMovement(UBehaviorTreeComponent& OwnerComp) const
{
    AAIController* AI = OwnerComp.GetAIOwner();
    if (!AI) return;

    AEnemy* Enemy = Cast<AEnemy>(AI->GetPawn());
    if (!Enemy) return;

    Enemy->StopHorizontalMovement();
}

EBTNodeResult::Type UBTTask_ChasePlayer::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    // 추적이 중단되어도 이전 이동 입력을 남기지 않는다.
    StopMovement(OwnerComp);
    return EBTNodeResult::Aborted;
}

void UBTTask_ChasePlayer::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* Memory, EBTNodeResult::Type Result)
{
    StopMovement(OwnerComp);
    Super::OnTaskFinished(OwnerComp, Memory, Result);
}
