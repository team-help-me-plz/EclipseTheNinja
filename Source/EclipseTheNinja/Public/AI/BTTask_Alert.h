#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_Alert.generated.h"

UCLASS(Blueprintable)
class ECLIPSETHENINJA_API UBTTask_Alert : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_Alert();
    virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector LastKnownLocationKey;
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector HasLastKnownLocationKey;

    /** 확정된 전투 추적보다 느린 경계 이동 속도. */
    UPROPERTY(EditAnywhere, Category = "Investigation", meta = (ClampMin = "1.0", Units = "cm/s"))
    float InvestigationSpeed = 100.0f;

    /** 마지막 관측 위치에 도착했다고 인정할 X축 거리. 새 관측 위치를 구분할 때도 사용한다. */
    UPROPERTY(EditAnywhere, Category = "Investigation", meta = (ClampMin = "5.0", Units = "cm"))
    float AcceptanceRadius = 30.0f;

    /** 마지막 관측 지점 주변을 짧게 이동하며 확인할 범위. */
    UPROPERTY(EditAnywhere, Category = "Investigation", meta = (ClampMin = "40.0", Units = "cm"))
    float LocalSearchRadius = 120.0f;

    UPROPERTY(EditAnywhere, Category = "Investigation", meta = (ClampMin = "1.0", Units = "cm/s"))
    float LocalSearchSpeed = 80.0f;

    UPROPERTY(EditAnywhere, Category = "Investigation", meta = (ClampMin = "0.0", Units = "s"))
    float SearchPauseDuration = 0.8f;
protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type Result) override;
private:
    EBTNodeResult::Type UpdateAlert(UBehaviorTreeComponent& OwnerComp, float DeltaSeconds);
    void UpdateLocalSearch(class AEnemy* Enemy, float DeltaSeconds);
    void UpdateBlockedTime(float CurrentX, float MoveSpeed, float DeltaSeconds);
    void StopMovement(UBehaviorTreeComponent& OwnerComp) const;
    void ChooseSearchDestination(const class AEnemy* Enemy);
    void BeginLocalSearch(class AEnemy* Enemy, const FVector& LastSeen, bool bApproachBlocked);

    // 주변 수색의 중심. 접근이 막히면 적의 현재 위치를 중심으로 사용한다.
    FVector SearchCenter = FVector::ZeroVector;
    // 현재 수색을 시작하게 한 관측 위치.
    FVector ObservedLocation = FVector::ZeroVector;
    // 경계를 시작할 때 한 번만 기록한 알아차린 위치. 이후 플레이어가 움직여도 따라가지 않는다.
    FVector NoticedLocation = FVector::ZeroVector;
    bool bHasNoticedLocation = false;
    float SearchDestinationX = 0.0f;
    // +1은 X축 오른쪽, -1은 왼쪽이다.
    float SearchDirection = 1.0f;
    float PauseRemaining = 0.0f;
    // 이전 갱신 시점의 X 좌표와, 거의 움직이지 못한 누적 시간.
    float PreviousX = 0.0f;
    float BlockedElapsed = 0.0f;
    // false: 관측 위치로 접근 중 / true: 주변 수색 중(대기 포함).
    bool bSearching = false;
};
