#include "AI/BTTask_Alert.h"

#include "AIController.h"
#include "Character/Enemy.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"

UBTTask_Alert::UBTTask_Alert()
{
    NodeName = TEXT("Investigate Last Seen Location");

    // 이 태스크는 수색 위치와 대기 시간을 멤버로 저장하므로 적마다 따로 생성한다.
    bCreateNodeInstance = true;
    INIT_TASK_NODE_NOTIFY_FLAGS();

    // BT 에디터에서 각 용도에 맞는 타입의 Blackboard 키만 선택하도록 제한한다.
    LastKnownLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_Alert, LastKnownLocationKey));
    HasLastKnownLocationKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_Alert, HasLastKnownLocationKey));

    LastKnownLocationKey.SelectedKeyName = TEXT("LastKnownLocation");
    HasLastKnownLocationKey.SelectedKeyName = TEXT("bHasLastKnownLocation");
}

void UBTTask_Alert::InitializeFromAsset(UBehaviorTree& Asset)
{
    Super::InitializeFromAsset(Asset);
    if (!Asset.BlackboardAsset) return;

    // 지정한 키 이름을 실제 Blackboard의 키 ID와 타입 정보에 연결한다.
    LastKnownLocationKey.ResolveSelectedKey(*Asset.BlackboardAsset);
    HasLastKnownLocationKey.ResolveSelectedKey(*Asset.BlackboardAsset);
}

EBTNodeResult::Type UBTTask_Alert::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    // 경계 행동을 새로 시작할 때 이전 실행의 수색 상태를 초기화한다.
    bSearching = false;
    PauseRemaining = 0.0f;
    BlockedElapsed = 0.0f;
    bHasNoticedLocation = false;

    // 첫 실행에서는 시간 경과 없이 상태와 관측 위치만 확인한다.
    return UpdateAlert(OwnerComp, 0.0f);
}

void UBTTask_Alert::TickTask(UBehaviorTreeComponent& OwnerComp, uint8*, float DeltaSeconds)
{
    const EBTNodeResult::Type Result = UpdateAlert(OwnerComp, DeltaSeconds);
    if (Result == EBTNodeResult::InProgress) return;

    // ExecuteTask에서 시작한 태스크를 종료하고 결과를 BT에 전달한다.
    FinishLatentTask(OwnerComp, Result);
}

EBTNodeResult::Type UBTTask_Alert::UpdateAlert(UBehaviorTreeComponent& OwnerComp, float DeltaSeconds)
{
    AAIController* AI = OwnerComp.GetAIOwner();
    if (!AI) return EBTNodeResult::Failed;

    AEnemy* Enemy = Cast<AEnemy>(AI->GetPawn());
    if (!Enemy) return EBTNodeResult::Failed;
    if (Enemy->IsDead()) return EBTNodeResult::Failed;

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return EBTNodeResult::Failed;

    // 키 이름뿐 아니라 실제 키 타입도 맞아야 값을 읽을 수 있다.
    if (LastKnownLocationKey.SelectedKeyType != UBlackboardKeyType_Vector::StaticClass())
    {
        return EBTNodeResult::Failed;
    }

    if (HasLastKnownLocationKey.SelectedKeyType != UBlackboardKeyType_Bool::StaticClass())
    {
        return EBTNodeResult::Failed;
    }

    if (!Enemy->IsReadyForAI()) return EBTNodeResult::InProgress;

    // 기억하는 위치가 없으면 이 태스크에서 수색할 대상도 없다.
    if (!BB->GetValueAsBool(HasLastKnownLocationKey.SelectedKeyName)) return EBTNodeResult::Failed;
    if (!BB->IsVectorValueSet(LastKnownLocationKey.SelectedKeyName)) return EBTNodeResult::Failed;

    Enemy->SetBehaviorState(EEnemyBehaviorState::Investigate);

    // 피격 중에는 이동과 수색 시간 처리를 잠시 멈춘다.
    if (Enemy->IsPlayingHitReaction())
    {
        Enemy->StopHorizontalMovement();
        return EBTNodeResult::InProgress;
    }

    // 경계를 시작할 때의 마지막 관측 위치(알아차린 지점)를 한 번만 기록한다.
    // 경계 중 플레이어가 보이며 관측 위치가 갱신되어도 따라가지 않고 이 지점만 수색한다.
    if (!bHasNoticedLocation)
    {
        NoticedLocation = BB->GetValueAsVector(LastKnownLocationKey.SelectedKeyName);
        bHasNoticedLocation = true;
    }
    const FVector LastSeen = NoticedLocation;
    const float CurrentX = Enemy->GetActorLocation().X;
    const float ArrivalDistance = FMath::Max(5.0f, AcceptanceRadius);

    // 1단계: 마지막 관측 위치로 접근한다.
    if (!bSearching)
    {
        const float DistanceToLastSeen = FMath::Abs(LastSeen.X - CurrentX);
        if (DistanceToLastSeen > ArrivalDistance)
        {
            // ExecuteTask의 첫 호출에서는 이동하지 않고 현재 위치만 기억한다.
            if (DeltaSeconds <= 0.0f)
            {
                PreviousX = CurrentX;
                return EBTNodeResult::InProgress;
            }

            const float ApproachSpeed = FMath::Max(1.0f, InvestigationSpeed);
            UpdateBlockedTime(CurrentX, ApproachSpeed, DeltaSeconds);

            // 1초간 거의 이동하지 못했다면 현재 위치 주변의 수색으로 전환한다.
            if (BlockedElapsed >= 1.0f)
            {
                BeginLocalSearch(Enemy, LastSeen, true);
                return EBTNodeResult::InProgress;
            }

            Enemy->MoveAlongX(LastSeen.X, ApproachSpeed, DeltaSeconds);
            return EBTNodeResult::InProgress;
        }

        // 관측 위치에 도착했으므로 그 위치 주변을 수색한다.
        BeginLocalSearch(Enemy, LastSeen, false);
    }

    // 2단계: 잠시 대기하고 주변 수색 지점 사이를 이동한다.
    UpdateLocalSearch(Enemy, DeltaSeconds);

    // 이 태스크는 계속 수색한다. 10초 후 기억 초기화와 Combat 전환은 감지 컴포넌트가 결정한다.
    // Blackboard가 바뀌면 BT의 Decorator가 이 태스크를 중단한다.
    return EBTNodeResult::InProgress;
}

void UBTTask_Alert::UpdateLocalSearch(AEnemy* Enemy, float DeltaSeconds)
{
    const float CurrentX = Enemy->GetActorLocation().X;

    // 도착 직후에는 같은 방향을 유지한 채 잠시 멈춘다.
    if (PauseRemaining > 0.0f)
    {
        Enemy->StopHorizontalMovement();
        PauseRemaining = FMath::Max(0.0f, PauseRemaining - DeltaSeconds);
        PreviousX = CurrentX;
        return;
    }

    const float SearchSpeed = FMath::Max(1.0f, LocalSearchSpeed);
    UpdateBlockedTime(CurrentX, SearchSpeed, DeltaSeconds);

    // 주변 수색 지점의 도착 허용 거리는 기존 동작대로 최대 15cm를 사용한다.
    const float ArrivalDistance = FMath::Max(5.0f, AcceptanceRadius);
    const float SearchArrivalDistance = FMath::Min(ArrivalDistance, 15.0f);
    const float DistanceToDestination = FMath::Abs(SearchDestinationX - CurrentX);

    bool bShouldChangeDestination = false;
    if (DistanceToDestination <= SearchArrivalDistance)
    {
        bShouldChangeDestination = true;
    }
    else if (BlockedElapsed >= 1.0f)
    {
        bShouldChangeDestination = true;
    }

    // 목적지에 도착했거나 막혔다면 반대쪽 목적지를 정하고 대기를 시작한다.
    if (bShouldChangeDestination)
    {
        Enemy->StopHorizontalMovement();
        SearchDirection = -SearchDirection;
        ChooseSearchDestination(Enemy);

        PauseRemaining = FMath::Max(0.0f, SearchPauseDuration);
        BlockedElapsed = 0.0f;
        return;
    }

    if (DeltaSeconds <= 0.0f) return;

    // 방향 전환은 실제 이동할 때만 한다. 대기 중에는 제자리 회전을 하지 않는다.
    Enemy->MoveAlongX(SearchDestinationX, SearchSpeed, DeltaSeconds);
}

void UBTTask_Alert::UpdateBlockedTime(float CurrentX, float MoveSpeed, float DeltaSeconds)
{
    const float ActualMoveDistance = FMath::Abs(CurrentX - PreviousX);

    // 기대 이동량의 10%보다 적게 움직였으면 진행하지 못한 것으로 판단한다.
    const float MinimumMoveDistance = FMath::Max(0.001f, MoveSpeed * DeltaSeconds * 0.1f);
    if (ActualMoveDistance < MinimumMoveDistance)
    {
        BlockedElapsed += DeltaSeconds;
    }
    else
    {
        BlockedElapsed = 0.0f;
    }

    PreviousX = CurrentX;
}

void UBTTask_Alert::BeginLocalSearch(AEnemy* Enemy, const FVector& LastSeen, bool bApproachBlocked)
{
    bSearching = true;
    ObservedLocation = LastSeen;

    // 정상 도착이면 관측 위치를, 접근이 막혔으면 현재 위치를 수색 중심으로 삼는다.
    if (bApproachBlocked)
    {
        SearchCenter = Enemy->GetActorLocation();

        // 막힌 목적지의 반대 방향부터 확인한다.
        const float DirectionToLastSeen = FMath::Sign(LastSeen.X - Enemy->GetActorLocation().X);
        SearchDirection = -DirectionToLastSeen;
    }
    else
    {
        SearchCenter = LastSeen;

        // 정상 도착 시에는 현재 바라보는 방향부터 확인한다.
        SearchDirection = 1.0f;
        if (Enemy->GetActorForwardVector().X < 0.0f)
        {
            SearchDirection = -1.0f;
        }
    }

    PauseRemaining = FMath::Max(0.0f, SearchPauseDuration);
    PreviousX = Enemy->GetActorLocation().X;
    BlockedElapsed = 0.0f;

    ChooseSearchDestination(Enemy);
    Enemy->StopHorizontalMovement();
}

void UBTTask_Alert::ChooseSearchDestination(const AEnemy* Enemy)
{
    const float SearchDistance = FMath::Max(40.0f, LocalSearchRadius);

    FVector CandidateLocation = Enemy->GetActorLocation();
    CandidateLocation.X = SearchCenter.X + SearchDirection * SearchDistance;

    // 선택한 수색 지점이 활동 영역 밖이면 반대쪽을 확인한다.
    if (!Enemy->IsInsideCombatArea(CandidateLocation))
    {
        SearchDirection = -SearchDirection;
        CandidateLocation.X = SearchCenter.X + SearchDirection * SearchDistance;
    }

    // 양쪽 모두 활동 영역 밖이면 현재 위치에서 멈춘다.
    if (!Enemy->IsInsideCombatArea(CandidateLocation))
    {
        SearchDestinationX = Enemy->GetActorLocation().X;
        return;
    }

    SearchDestinationX = CandidateLocation.X;
}

void UBTTask_Alert::StopMovement(UBehaviorTreeComponent& OwnerComp) const
{
    AAIController* AI = OwnerComp.GetAIOwner();
    if (!AI) return;

    AEnemy* Enemy = Cast<AEnemy>(AI->GetPawn());
    if (!Enemy) return;

    Enemy->StopHorizontalMovement();
}

EBTNodeResult::Type UBTTask_Alert::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    // 다른 BT 분기로 전환될 때 이전 수색 이동이 남지 않도록 정지한다.
    StopMovement(OwnerComp);
    return EBTNodeResult::Aborted;
}

void UBTTask_Alert::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* Memory, EBTNodeResult::Type Result)
{
    StopMovement(OwnerComp);
    Super::OnTaskFinished(OwnerComp, Memory, Result);
}
