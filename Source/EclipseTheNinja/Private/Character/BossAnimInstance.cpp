#include "Character/BossAnimInstance.h"

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
		WalkPlayRate = 1.0f;
		RunPlayRate = 1.0f;
		return;
	}

	GroundSpeed = Enemy->GetVelocity().Size2D();
	bIsMoving = GroundSpeed > MoveThreshold;

	// 전투 단계(대상 추격, 마지막 목격 위치로 이동)에서는 달리고, 그 외의 이동은 걷는다.
	const UEnemyDetectionComponent* Detection = Enemy->GetEneymDetectionComponent();
	const bool bInCombat = Detection && Detection->GetCurrentAlertLevel() == EEnemyAlertLevel::Combat;
	bIsRunning = bIsMoving && bInCombat;
	bIsWalking = bIsMoving && !bInCombat;

	WalkPlayRate = CalculatePlayRate(WalkAnimSpeed);
	RunPlayRate = CalculatePlayRate(RunAnimSpeed);
}

float UBossAnimInstance::CalculatePlayRate(float AnimSpeed) const
{
	return FMath::Clamp(GroundSpeed / FMath::Max(1.0f, AnimSpeed), 0.5f, 2.0f);
}
