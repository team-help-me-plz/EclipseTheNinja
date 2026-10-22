#include "Character/KunaiEnemy.h"

#include "Component/EnemyDetectionComponent.h"
#include "Engine/World.h"

bool AKunaiEnemy::IsWithinAttackRange(AActor* Target) const
{
	if (!IsValidAITarget(Target)) return false;
	// 이동은 XZ 평면에서 좌우로만 수행하므로 정지 구간은 높이 차이를 제외한 X축 거리로 판단한다.
	const float Distance = FMath::Abs(Target->GetActorLocation().X - GetActorLocation().X);
	const float Desired = FMath::Max(100.0f, KunaiKeepDistance);
	const float Tolerance = FMath::Clamp(KunaiDistanceTolerance, 10.0f, Desired * 0.5f);
	return FMath::Abs(Distance - Desired) <= Tolerance;
}

bool AKunaiEnemy::CanStartAIAttack(AActor* Target) const
{
	// 목표 거리 도달 조건을 넣지 않아, 후퇴 중에도 쿨타임이 끝나면 현재 위치에서 투척할 수 있다.
	// 전투 반경은 실제 감지 검사와 동일한 3차원 거리로 비교한다. GAS의 세부 활성화 조건은 이후에 검사한다.
	return IsValidAITarget(Target) && !IsPlayingHitReaction() && !IsAttackActive()
		&& GetWorld()->GetTimeSeconds() >= NextAttackTime
		&& FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(GetCombatSightRadius());
}

void AKunaiEnemy::UpdateCombatMovement(AActor* Target, float DeltaSeconds)
{
	if (!IsValidAITarget(Target) || IsPlayingHitReaction() || IsAttackActive() || IsWithinAttackRange(Target))
	{
		StopHorizontalMovement();
		return;
	}
	const float OffsetX = GetActorLocation().X - Target->GetActorLocation().X;
	// 현재 대상의 왼쪽/오른쪽에 있는 방향을 유지하며 거리를 벌린다.
	// X좌표가 정확히 같으면 몸이 바라보는 방향의 뒤쪽을 선택해 이동 방향이 0이 되는 것을 방지한다.
	const float Side = FMath::IsNearlyZero(OffsetX) ? -GetActorForwardVector().X : FMath::Sign(OffsetX);
	const float DestinationX = Target->GetActorLocation().X + Side * FMath::Max(100.0f, KunaiKeepDistance);
	MoveAlongX(DestinationX, ChaseSpeed, DeltaSeconds);
	// MoveAlongX는 이동 방향으로 몸을 돌리므로, 후퇴 시에도 대상을 바라보도록 다시 정렬한다.
	FaceLocation(Target->GetActorLocation());
}

float AKunaiEnemy::GetCombatSightRadius() const
{
	// 컴포넌트가 현재 경계 단계의 배율을 적용한다. 이 클래스에서 감지 수치나 UI 상태를 직접 변경하지 않는다.
	return EnemyDetectComponent ? EnemyDetectComponent->GetCurrentDetectionRadius() : 0.0f;
}

bool AKunaiEnemy::ActivateAIAttack()
{
	// 어빌리티 클래스·몽타주·표창 참조는 Blueprint가 담당하고, 여기서는 지정된 입력 ID로 활성화만 요청한다.
	return TryActivateAbilityByInputID(GetAIAttackInputID());
}
