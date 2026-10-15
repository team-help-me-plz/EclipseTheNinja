#include "AI/BTTask_ChoosePatrolLocation.h"

#include "Character/Enemy.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"

UBTTask_ChoosePatrolLocation::UBTTask_ChoosePatrolLocation()
{
    NodeName = TEXT("Choose Patrol Location");
    // 초기화 대기 중 TickTask가 호출되도록 알림 플래그를 설정한다.
    INIT_TASK_NODE_NOTIFY_FLAGS();

    // 키 셀렉터에 벡터값만 들어가는 필터를 생성
    PatrolLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_ChoosePatrolLocation, PatrolLocationKey));

    // 기본으로 사용할 키 이름 지정
    PatrolLocationKey.SelectedKeyName = TEXT("PatrolLocation");
}

void UBTTask_ChoosePatrolLocation::InitializeFromAsset(UBehaviorTree& Asset)
{
    Super::InitializeFromAsset(Asset);
    if (!Asset.BlackboardAsset) return;

    // 선택한 키 이름을 실제 Blackboard의 키 ID와 타입 정보에 연결한다.
    PatrolLocationKey.ResolveSelectedKey(*Asset.BlackboardAsset);
}

EBTNodeResult::Type UBTTask_ChoosePatrolLocation::SelectDestination(UBehaviorTreeComponent& OwnerComp) const
{
    AAIController* AI = OwnerComp.GetAIOwner();
    if (!AI) return EBTNodeResult::Failed;

    AEnemy* Enemy = Cast<AEnemy>(AI->GetPawn());
    if (!Enemy) return EBTNodeResult::Failed;
    if (Enemy->IsDead()) return EBTNodeResult::Failed;

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return EBTNodeResult::Failed;

    // 목적지를 저장할 키는 Vector 타입이어야 한다.
    if (PatrolLocationKey.SelectedKeyType != UBlackboardKeyType_Vector::StaticClass())
    {
        return EBTNodeResult::Failed;
    }

    // 적의 초기화가 끝날 때까지 기다린다.
    if (!Enemy->IsReadyForAI()) return EBTNodeResult::InProgress;

    // 다음 순찰 목적지를 계산하고 Blackboard에 기록한다.
    const FVector PatrolLocation = Enemy->ChooseNextPatrolLocation();
    BB->SetValueAsVector(PatrolLocationKey.SelectedKeyName, PatrolLocation);

    return EBTNodeResult::Succeeded;
}

EBTNodeResult::Type UBTTask_ChoosePatrolLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    return SelectDestination(OwnerComp);
}

void UBTTask_ChoosePatrolLocation::TickTask(UBehaviorTreeComponent& OwnerComp, uint8*, float)
{
    const EBTNodeResult::Type Result = SelectDestination(OwnerComp);

    if (Result == EBTNodeResult::InProgress) return;

    // 초기화 대기가 끝나면 목적지 선택 결과를 BT에 전달한다.
    FinishLatentTask(OwnerComp, Result);
}
