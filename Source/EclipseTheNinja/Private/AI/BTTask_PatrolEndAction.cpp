#include "AI/BTTask_PatrolEndAction.h"

#include "Character/Enemy.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UBTTask_PatrolEndAction::UBTTask_PatrolEndAction()
{
    NodeName = TEXT("Patrol End Idle And Turn");

    // 몽타주 끝 알림과 대기 시간을 노드별 상태로 들고 있어야 하므로 인스턴스를 만든다.
    bCreateNodeInstance = true;
    INIT_TASK_NODE_NOTIFY_FLAGS();
}

EBTNodeResult::Type UBTTask_PatrolEndAction::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    OwnerCompRef = &OwnerComp;
    Phase = EPhase::None;
    WaitRemaining = 0.0f;
    TurnDirectionX = 0.0f;

    AAIController* AI = OwnerComp.GetAIOwner();
    AEnemy* Enemy = AI ? Cast<AEnemy>(AI->GetPawn()) : nullptr;

    if (!Enemy || Enemy->IsDead())
    {
        return EBTNodeResult::Failed;
    }

    EnemyRef = Enemy;

    // 순찰 이동이 남긴 수평 속도를 멈추고 제자리에서 대기한다.
    Enemy->StopHorizontalMovement();

    return StartIdle();
}

void UBTTask_PatrolEndAction::TickTask(UBehaviorTreeComponent&, uint8*, float DeltaSeconds)
{
    // 대기 몽타주가 없는 적만 시간을 잰다.
    if (Phase != EPhase::Idle || WaitRemaining <= 0.0f)
    {
        return;
    }

    WaitRemaining -= DeltaSeconds;

    if (WaitRemaining > 0.0f)
    {
        return;
    }

    WaitRemaining = 0.0f;
    FinishIfDone(StartTurn());
}

EBTNodeResult::Type UBTTask_PatrolEndAction::AbortTask(UBehaviorTreeComponent&, uint8*)
{
    const bool bWasTurning = Phase == EPhase::Turn;

    // 멈추는 몽타주의 블렌드 아웃 동안 남은 루트 회전이 맞춘 방향을 흐트러뜨리지 않게 한다.
    if (bWasTurning)
    {
        bTurnSnapped = true;
    }

    StopPhaseMontage();

    // 도는 도중에 끊기면 가까운 쪽 좌우 방향으로 맞춘다. 비스듬히 선 채 피격·경계로 넘어가지 않게 한다.
    AEnemy* Enemy = EnemyRef.Get();

    if (bWasTurning && Enemy)
    {
        Enemy->FaceDirection(Enemy->GetActorForwardVector().X);
    }

    return EBTNodeResult::Aborted;
}

void UBTTask_PatrolEndAction::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type Result)
{
    StopPhaseMontage();

    Phase = EPhase::None;
    WaitRemaining = 0.0f;
    EnemyRef.Reset();
    OwnerCompRef.Reset();

    Super::OnTaskFinished(OwnerComp, NodeMemory, Result);
}

EBTNodeResult::Type UBTTask_PatrolEndAction::StartIdle()
{
    AEnemy* Enemy = EnemyRef.Get();

    if (!Enemy)
    {
        return EBTNodeResult::Failed;
    }

    if (PlayPhaseMontage(Enemy->GetPatrolEndIdleMontage(), EPhase::Idle))
    {
        return EBTNodeResult::InProgress;
    }

    // 대기 몽타주가 없으면 기존 Wait 노드처럼 무작위 시간만큼 서 있는다.
    Phase = EPhase::Idle;
    WaitRemaining = FMath::Max(
        0.0f,
        FallbackWaitTime + FMath::FRandRange(-FallbackWaitDeviation, FallbackWaitDeviation)
    );

    if (WaitRemaining > 0.0f)
    {
        return EBTNodeResult::InProgress;
    }

    return StartTurn();
}

EBTNodeResult::Type UBTTask_PatrolEndAction::StartTurn()
{
    AEnemy* Enemy = EnemyRef.Get();

    if (!Enemy || Enemy->IsDead())
    {
        return EBTNodeResult::Failed;
    }

    // 다음 순찰 방향을 여기서 확정한다. 회전 중 순찰 끝에서 밀려나도 다음 목적지가 되돌아가지 않는다.
    // 다음 목적지가 이미 앞쪽이면 돌지 않는다.
    TurnDirectionX = Enemy->CommitNextPatrolDirectionX();
    const bool bAlreadyFacing = FMath::IsNearlyZero(TurnDirectionX)
        || Enemy->GetActorForwardVector().X * TurnDirectionX > 0.0f;

    if (bAlreadyFacing)
    {
        return EBTNodeResult::Succeeded;
    }

    // 회전은 몽타주의 루트 모션이 액터를 돌린다. 끝 알림에서 정확한 방향으로 맞춘다.
    if (PlayPhaseMontage(Enemy->GetPatrolTurnMontage(), EPhase::Turn))
    {
        bTurnSnapped = false;
        SetTurnRootMotionFilter(true);

        // 블렌드 아웃까지 완전히 끝나면 필터를 푼다. 태스크가 먼저 끝나도 남은 블렌드 아웃 동안 위치가 밀리지 않게 한다.
        FOnMontageEnded EndDelegate = FOnMontageEnded::CreateWeakLambda(
            this,
            [this](UAnimMontage* EndedMontage, bool)
            {
                // 이전 회전의 끝 알림이 새로 시작한 회전의 필터를 풀지 않게 한다.
                if (EndedMontage && EndedMontage == PlayingMontage.Get())
                {
                    return;
                }

                SetTurnRootMotionFilter(false);
            }
        );
        GetEnemyAnimInstance()->Montage_SetEndDelegate(EndDelegate, Enemy->GetPatrolTurnMontage());

        return EBTNodeResult::InProgress;
    }

    // 회전 몽타주가 없으면 기존처럼 즉시 돈다.
    Enemy->FaceDirection(TurnDirectionX);

    return EBTNodeResult::Succeeded;
}

bool UBTTask_PatrolEndAction::PlayPhaseMontage(UAnimMontage* Montage, EPhase NewPhase)
{
    UAnimInstance* AnimInstance = GetEnemyAnimInstance();

    if (!Montage || !AnimInstance)
    {
        return false;
    }

    if (AnimInstance->Montage_Play(Montage) <= 0.0f)
    {
        return false;
    }

    PlayingMontage = Montage;
    Phase = NewPhase;

    // 블렌드 아웃이 시작될 때 다음 단계로 넘어가 대기와 회전 사이에 멈칫하는 틈이 없게 한다.
    FOnMontageBlendingOutStarted BlendOutDelegate = FOnMontageBlendingOutStarted::CreateUObject(
        this,
        &UBTTask_PatrolEndAction::OnMontageBlendingOut
    );
    AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, Montage);

    return true;
}

void UBTTask_PatrolEndAction::StopPhaseMontage()
{
    UAnimMontage* Montage = PlayingMontage.Get();

    // 먼저 비워서 아래 Montage_Stop이 보내는 끝 알림을 무시하게 한다.
    PlayingMontage.Reset();

    UAnimInstance* AnimInstance = GetEnemyAnimInstance();

    if (Montage && AnimInstance && AnimInstance->Montage_IsPlaying(Montage))
    {
        AnimInstance->Montage_Stop(0.2f, Montage);
    }
}

void UBTTask_PatrolEndAction::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
    // 이 태스크가 재생 중인 몽타주의 알림만 처리한다.
    if (!Montage || Montage != PlayingMontage.Get())
    {
        return;
    }

    PlayingMontage.Reset();

    AEnemy* Enemy = EnemyRef.Get();

    // 회전 몽타주가 블렌드 아웃에 들어가면 방향을 맞추므로, 남은 루트 회전은 버린다.
    if (Phase == EPhase::Turn)
    {
        bTurnSnapped = true;
    }

    // 피격·사망 등 다른 몽타주에 끊기면 순찰 끝 동작을 그만둔다.
    if (bInterrupted || !Enemy)
    {
        if (Phase == EPhase::Turn && Enemy)
        {
            Enemy->FaceDirection(Enemy->GetActorForwardVector().X);
        }

        FinishIfDone(EBTNodeResult::Failed);
        return;
    }

    if (Phase == EPhase::Idle)
    {
        FinishIfDone(StartTurn());
        return;
    }

    // 루트 모션 회전이 블렌드 아웃으로 조금 덜 돌았을 수 있으므로 정확한 좌우 방향으로 맞춘다.
    Enemy->FaceDirection(TurnDirectionX);
    FinishIfDone(EBTNodeResult::Succeeded);
}

void UBTTask_PatrolEndAction::FinishIfDone(EBTNodeResult::Type Result)
{
    UBehaviorTreeComponent* OwnerComp = OwnerCompRef.Get();

    if (Result == EBTNodeResult::InProgress || !OwnerComp)
    {
        return;
    }

    FinishLatentTask(*OwnerComp, Result);
}

UAnimInstance* UBTTask_PatrolEndAction::GetEnemyAnimInstance() const
{
    const AEnemy* Enemy = EnemyRef.Get();

    if (!Enemy || !Enemy->GetMesh())
    {
        return nullptr;
    }

    return Enemy->GetMesh()->GetAnimInstance();
}

void UBTTask_PatrolEndAction::SetTurnRootMotionFilter(bool bEnable)
{
    // 이전에 건 필터는 항상 먼저 푼다. 다른 시스템이 건 바인딩은 건드리지 않는다.
    UCharacterMovementComponent* OldMovement = FilteredMovement.Get();

    if (OldMovement && OldMovement->ProcessRootMotionPostConvertToWorld.IsBoundToObject(this))
    {
        OldMovement->ProcessRootMotionPostConvertToWorld.Unbind();
    }

    FilteredMovement.Reset();

    const AEnemy* Enemy = EnemyRef.Get();
    UCharacterMovementComponent* Movement = Enemy ? Enemy->GetCharacterMovement() : nullptr;

    if (!bEnable || !Movement)
    {
        return;
    }

    Movement->ProcessRootMotionPostConvertToWorld.BindUObject(
        this,
        &UBTTask_PatrolEndAction::FilterTurnRootMotion
    );
    FilteredMovement = Movement;
}

FTransform UBTTask_PatrolEndAction::FilterTurnRootMotion(
    const FTransform& WorldRootMotion,
    UCharacterMovementComponent* Movement,
    float DeltaSeconds
) const
{
    // 회전 몽타주가 아닌 루트 모션(피격·공격 등)은 그대로 둔다.
    const ACharacter* Character = Movement ? Movement->GetCharacterOwner() : nullptr;
    const UAnimInstance* AnimInstance = Character && Character->GetMesh()
        ? Character->GetMesh()->GetAnimInstance()
        : nullptr;
    const AEnemy* Enemy = Cast<AEnemy>(Character);
    const UAnimMontage* TurnMontage = Enemy ? Enemy->GetPatrolTurnMontage() : nullptr;

    if (!AnimInstance || !TurnMontage)
    {
        return WorldRootMotion;
    }

    // 다른 루트 모션 몽타주가 주도하고 있으면 건드리지 않는다.
    const FAnimMontageInstance* RootMotionInstance = AnimInstance->GetRootMotionMontageInstance();

    if (RootMotionInstance && RootMotionInstance->Montage != TurnMontage)
    {
        return WorldRootMotion;
    }

    // 블렌드 아웃 중인 인스턴스까지 포함해 회전 몽타주가 남아 있는지 본다.
    const bool bTurnMontageAlive = AnimInstance->MontageInstances.ContainsByPredicate(
        [TurnMontage](const FAnimMontageInstance* Instance)
        {
            return Instance && Instance->Montage == TurnMontage;
        }
    );

    if (!bTurnMontageAlive)
    {
        return WorldRootMotion;
    }

    // 방향을 맞춘 뒤의 블렌드 아웃 구간은 회전도 버린다.
    if (bTurnSnapped)
    {
        return FTransform::Identity;
    }

    // 월드 변환을 거친 뒤에 이동을 버린다. 엔진은 메시 원점을 축으로 돌리느라 이동을 더하는데,
    // 메시가 캡슐에서 떨어져 있어 그 이동만큼 미끄러진다. 캡슐 중심에서 회전만 하게 한다.
    return FTransform(WorldRootMotion.GetRotation());
}
