#include "Component/EnemyFloorActivationComponent.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Character/Enemy.h"
#include "Component/EnemyDetectionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Subsystem/EnemyFloorSubsystem.h"

UEnemyFloorActivationComponent::UEnemyFloorActivationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyFloorActivationComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerEnemy = Cast<AEnemy>(GetOwner());
	if (!OwnerEnemy.IsValid())
	{
		return;
	}

	HomeLocation = OwnerEnemy->GetActorLocation();

	if (UEnemyFloorSubsystem* FloorSubsystem = GetWorld()->GetSubsystem<UEnemyFloorSubsystem>())
	{
		FloorSubsystem->RegisterEnemy(this);
	}
}

void UEnemyFloorActivationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 기믹 방처럼 숨겼다 다시 보이는 레벨은 BeginPlay가 다시 불린다. 멈춘 상태를 남기지 않는다.
	ExitDormancy();

	if (UWorld* World = GetWorld())
	{
		if (UEnemyFloorSubsystem* FloorSubsystem = World->GetSubsystem<UEnemyFloorSubsystem>())
		{
			FloorSubsystem->UnregisterEnemy(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UEnemyFloorActivationComponent::Evaluate(int32 PlayerFloor)
{
	if (!bUseFloorActivation)
	{
		ExitDormancy();
		return;
	}

	const UEnemyFloorSubsystem* FloorSubsystem = GetWorld()->GetSubsystem<UEnemyFloorSubsystem>();
	if (!FloorSubsystem)
	{
		return;
	}

	// 층을 모르면(볼륨 밖 적, 플레이어 층 미확정) 항상 활동한다.
	const int32 MyFloor = FloorSubsystem->FindFloorAt(HomeLocation);
	bool bShouldBeActive = true;
	if (MyFloor != INDEX_NONE && PlayerFloor != INDEX_NONE)
	{
		bShouldBeActive = FMath::Abs(MyFloor - PlayerFloor) <= ActiveFloorRange;
	}

	if (bShouldBeActive)
	{
		ExitDormancy();
		return;
	}

	// 조건이 안 맞으면 이번에는 넘기고 다음 판정에서 다시 확인한다.
	if (!bDormant && CanEnterDormancy())
	{
		EnterDormancy();
	}
}

void UEnemyFloorActivationComponent::WakeUp()
{
	ExitDormancy();
}

void UEnemyFloorActivationComponent::HandleOwnerReset()
{
	ExitDormancy();

	// 리셋·풀 스폰은 위치를 먼저 옮긴 뒤 불린다. 새 위치를 층 판정 기준으로 삼는다.
	if (OwnerEnemy.IsValid())
	{
		HomeLocation = OwnerEnemy->GetActorLocation();
	}
}

void UEnemyFloorActivationComponent::HandleReturnToPool()
{
	// 사망 상태라 액터 틱과 AI는 되돌리지 않는다. 컴포넌트 틱은 이어서 풀이 모두 끈다.
	ExitDormancy();
}

bool UEnemyFloorActivationComponent::CanEnterDormancy() const
{
	const AEnemy* Enemy = OwnerEnemy.Get();

	// 사망·풀 대기·암살 중이거나 아직 BeginPlay 전이면 각자의 흐름에 맡긴다.
	if (!Enemy || !Enemy->IsReadyForAI())
	{
		return false;
	}

	// 공격·피격 몽타주나 어빌리티 도중에 멈추면 타이머와 몽타주가 어긋난다.
	if (Enemy->IsAttackActive() || Enemy->IsPlayingHitReaction())
	{
		return false;
	}

	// 공중에서 멈추면 깨어날 때까지 떠 있게 된다.
	const UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement();
	if (Movement && Movement->IsFalling())
	{
		return false;
	}

	// 경계·전투·소리 반응 중이면 진정될 때까지 활동한다. 플레이어 교전 목록도 여기서 정리된다.
	const UEnemyDetectionComponent* Detection = Enemy->GetEneymDetectionComponent();
	if (!Detection || !Detection->IsCalm())
	{
		return false;
	}

	// 암살·연출 등 다른 시스템이 이미 AI를 멈췄으면 손대지 않는다. 그 시스템이 다시 켤 때 어긋나지 않게 한다.
	const AAIController* AIController = Cast<AAIController>(Enemy->GetController());
	const UBrainComponent* Brain = AIController ? AIController->GetBrainComponent() : nullptr;
	if (!Brain || !Brain->IsRunning() || Brain->IsPaused())
	{
		return false;
	}

	return true;
}

void UEnemyFloorActivationComponent::EnterDormancy()
{
	AEnemy* Enemy = OwnerEnemy.Get();
	if (!Enemy || bDormant)
	{
		return;
	}

	bDormant = true;

	// 1. 감지 판정과 발각 범위 위젯을 먼저 멈춘다. 이후 Blackboard가 바뀌지 않는다.
	if (UEnemyDetectionComponent* Detection = Enemy->GetEneymDetectionComponent())
	{
		Detection->SetDormant(true);
	}

	// 2. AI: 진행 중인 이동 요청과 BT를 일시정지한다. 정지(Stop)가 아니라 깨어나면 그대로 이어진다.
	if (AAIController* AIController = Cast<AAIController>(Enemy->GetController()))
	{
		if (AIController->GetMoveStatus() == EPathFollowingStatus::Moving)
		{
			PausedMoveRequestId = AIController->GetCurrentMoveRequestID();
			bPausedMove = AIController->PauseMove(PausedMoveRequestId);
		}

		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->PauseLogic(TEXT("FloorDormant"));
			bPausedBrain = true;
		}

		if (AIController->IsActorTickEnabled())
		{
			AIController->SetActorTickEnabled(false);
			bSuspendedControllerTick = true;
		}
	}

	// 3. 적 액터 틱(파생 BP의 Tick 포함)
	if (Enemy->IsActorTickEnabled())
	{
		Enemy->SetActorTickEnabled(false);
		bSuspendedOwnerTick = true;
	}

	// 4. 비용이 큰 컴포넌트 틱: 애니메이션(스켈레탈 메시), 이동, 위젯
	TInlineComponentArray<UActorComponent*> Components(Enemy);
	for (UActorComponent* Component : Components)
	{
		const bool bTarget = Component->IsA<USkeletalMeshComponent>()
			|| Component->IsA<UCharacterMovementComponent>()
			|| Component->IsA<UWidgetComponent>();
		if (bTarget)
		{
			SuspendComponentTick(Component);
		}
	}
}

void UEnemyFloorActivationComponent::SuspendComponentTick(UActorComponent* Component)
{
	if (!Component || !Component->IsComponentTickEnabled())
	{
		return;
	}

	Component->SetComponentTickEnabled(false);
	SuspendedComponents.Add(Component);
}

void UEnemyFloorActivationComponent::ExitDormancy()
{
	if (!bDormant)
	{
		return;
	}

	bDormant = false;

	AEnemy* Enemy = OwnerEnemy.Get();
	const bool bOwnerDead = !Enemy || Enemy->IsDead();

	// 끈 순서의 역순으로 되돌린다. 먼저 몸(컴포넌트)을 깨운다.
	for (const TWeakObjectPtr<UActorComponent>& ComponentPtr : SuspendedComponents)
	{
		if (UActorComponent* Component = ComponentPtr.Get())
		{
			Component->SetComponentTickEnabled(true);
		}
	}
	SuspendedComponents.Reset();

	// 사망 처리가 끈 액터 틱은 되살리지 않는다. 리셋이 직접 켠다.
	if (bSuspendedOwnerTick && !bOwnerDead)
	{
		Enemy->SetActorTickEnabled(true);
	}
	bSuspendedOwnerTick = false;

	AAIController* AIController = Enemy ? Cast<AAIController>(Enemy->GetController()) : nullptr;
	if (AIController)
	{
		if (bSuspendedControllerTick)
		{
			AIController->SetActorTickEnabled(true);
		}

		// 내가 일시정지한 BT만 재개한다. 사망 시에는 StopLogic 상태라 리셋의 RestartAI가 다시 시작한다.
		UBrainComponent* Brain = AIController->GetBrainComponent();
		if (bPausedBrain && Brain && Brain->IsPaused() && !bOwnerDead)
		{
			Brain->ResumeLogic(TEXT("FloorDormant"));
		}

		if (bPausedMove && !bOwnerDead)
		{
			AIController->ResumeMove(PausedMoveRequestId);
		}
	}
	bSuspendedControllerTick = false;
	bPausedBrain = false;
	bPausedMove = false;
	PausedMoveRequestId = FAIRequestID::InvalidRequest;

	// 마지막으로 감지를 재개한다. 깨어난 직후의 판정이 위에서 되살린 AI에 바로 전달된다.
	if (Enemy)
	{
		if (UEnemyDetectionComponent* Detection = Enemy->GetEneymDetectionComponent())
		{
			Detection->SetDormant(false);
		}
	}
}
