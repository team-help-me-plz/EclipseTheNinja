#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PatrolEndAction.generated.h"

class AEnemy;
class UAnimInstance;
class UAnimMontage;
class UCharacterMovementComponent;

/**
 * 순찰 끝에 도착했을 때 대기 몽타주를 한 번 재생하고, 다음 목적지가 뒤쪽이면 회전 몽타주로 뒤돈다.
 * 몽타주는 적 BP의 PatrolEndIdleMontage·PatrolTurnMontage에서 가져온다. 대기 몽타주가 없으면 대기 시간만큼 서 있는다.
 */
UCLASS(Blueprintable)
class ECLIPSETHENINJA_API UBTTask_PatrolEndAction : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_PatrolEndAction();

    /** 대기 몽타주가 없는 적이 순찰 끝에서 서 있을 시간. */
    UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0", Units = "s"))
    float FallbackWaitTime = 2.5f;

    /** FallbackWaitTime에 더하거나 뺄 무작위 범위. */
    UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0", Units = "s"))
    float FallbackWaitDeviation = 1.0f;

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type Result) override;

private:
    enum class EPhase : uint8
    {
        None,
        Idle,
        Turn
    };

    // 대기 몽타주를 재생하거나, 없으면 대기 시간을 잰다.
    EBTNodeResult::Type StartIdle();

    // 다음 목적지가 뒤쪽이면 회전 몽타주를 재생한다. 돌 필요가 없거나 몽타주가 없으면 바로 끝낸다.
    EBTNodeResult::Type StartTurn();

    // 몽타주를 재생하고 끝 알림을 받는다. 재생하지 못하면 false.
    bool PlayPhaseMontage(UAnimMontage* Montage, EPhase NewPhase);

    // 재생 중인 이 태스크의 몽타주를 끝 알림 없이 멈춘다.
    void StopPhaseMontage();

    // 대기가 끝나면 회전으로, 회전이 끝나면 방향을 맞추고 태스크를 끝낸다.
    void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

    // 몽타주 끝 알림처럼 ExecuteTask 밖에서 결과가 정해졌을 때 BT에 전달한다.
    void FinishIfDone(EBTNodeResult::Type Result);

    UAnimInstance* GetEnemyAnimInstance() const;

    // 회전 몽타주의 루트 모션 중 회전만 쓰도록 이동 컴포넌트에 필터를 걸거나 푼다.
    void SetTurnRootMotionFilter(bool bEnable);

    // 루트 모션에서 이동을 버리고 회전만 남긴다. 회전이 끝난 뒤의 블렌드 아웃 구간은 회전도 버린다.
    FTransform FilterTurnRootMotion(const FTransform& WorldRootMotion, UCharacterMovementComponent* Movement, float DeltaSeconds) const;

    TWeakObjectPtr<UBehaviorTreeComponent> OwnerCompRef;
    TWeakObjectPtr<AEnemy> EnemyRef;

    // 이 태스크가 재생한 몽타주. 다른 몽타주의 끝 알림과 구분한다.
    TWeakObjectPtr<UAnimMontage> PlayingMontage;

    EPhase Phase = EPhase::None;

    // 대기 몽타주가 없을 때 남은 대기 시간. 0보다 크면 TickTask에서 줄인다.
    float WaitRemaining = 0.0f;

    // 회전을 시작할 때 정한 다음 목적지 방향. 회전이 끝나면 이 방향으로 정확히 맞춘다.
    float TurnDirectionX = 0.0f;

    // 루트 모션 필터를 건 이동 컴포넌트. 태스크가 끝나도 몽타주가 완전히 끝날 때까지 유지한다.
    TWeakObjectPtr<UCharacterMovementComponent> FilteredMovement;

    // 회전 몽타주가 블렌드 아웃에 들어가 방향을 맞춘 뒤인지. 이후 루트 모션 회전을 버린다.
    bool bTurnSnapped = false;
};
