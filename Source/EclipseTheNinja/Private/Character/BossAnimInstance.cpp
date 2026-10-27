#include "Character/BossAnimInstance.h"

#include "Character/BossEnemy.h"
#include "Character/Enemy.h"
#include "Component/EnemyDetectionComponent.h"

void UBossAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 에디터 프리뷰처럼 소유 폰이 없으면 정지 상태로 둔다.
	const AEnemy* Enemy = Cast<AEnemy>(TryGetPawnOwner());
	if (!IsValid(Enemy))
	{
		GroundSpeed = 0.0f;
		bIsMoving = false;
		bIsWalking = false;
		bIsRunning = false;
		StanceWalkAlpha = 0.0f;
		StanceWalkBackwardAlpha = 0.0f;
		WalkPlayRate = 1.0f;
		RunPlayRate = 1.0f;
		StanceWalkPlayRate = 1.0f;
		return;
	}

	const FVector Velocity = Enemy->GetVelocity();
	GroundSpeed = Velocity.Size2D();
	bIsMoving = GroundSpeed > MoveThreshold;

	// 전투 단계(대상 추격, 마지막 목격 위치로 이동)에서는 Run 상태, 그 외의 이동은 Walk 상태다.
	const UEnemyDetectionComponent* Detection = Enemy->GetEneymDetectionComponent();
	const bool bInCombat = Detection && Detection->GetCurrentAlertLevel() == EEnemyAlertLevel::Combat;
	bIsRunning = bIsMoving && bInCombat;
	bIsWalking = bIsMoving && !bInCombat;

	// 보스가 달리기 페이즈 전이면 전투 이동을 자세 걷기로 덮는다.
	const ABossEnemy* Boss = Cast<ABossEnemy>(Enemy);
	const bool bStanceWalking = bIsRunning && Boss && !Boss->IsRunPhase();
	StanceWalkAlpha = BlendAlphaTo(StanceWalkAlpha, bStanceWalking ? 1.0f : 0.0f, DeltaSeconds);

	// 바라보는 방향과 반대로 움직이면 뒷걸음질. 멈춰 있을 때는 마지막 방향을 유지한다.
	if (bIsMoving)
	{
		const bool bMovingBackward = FVector::DotProduct(Velocity.GetSafeNormal2D(), Enemy->GetActorForwardVector()) < 0.0f;
		StanceWalkBackwardAlpha = BlendAlphaTo(StanceWalkBackwardAlpha, bMovingBackward ? 1.0f : 0.0f, DeltaSeconds);
	}

	WalkPlayRate = CalculatePlayRate(WalkAnimSpeed);
	RunPlayRate = CalculatePlayRate(RunAnimSpeed);
	StanceWalkPlayRate = CalculatePlayRate(StanceWalkAnimSpeed);
}

float UBossAnimInstance::CalculatePlayRate(float AnimSpeed) const
{
	return FMath::Clamp(GroundSpeed / FMath::Max(1.0f, AnimSpeed), 0.5f, 2.0f);
}

float UBossAnimInstance::BlendAlphaTo(float Current, float Target, float DeltaSeconds) const
{
	if (StanceWalkBlendTime <= 0.0f)
	{
		return Target;
	}

	return FMath::FInterpConstantTo(Current, Target, DeltaSeconds, 1.0f / StanceWalkBlendTime);
}
