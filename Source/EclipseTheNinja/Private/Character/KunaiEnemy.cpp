#include "Character/KunaiEnemy.h"

#include "Component/EnemyDetectionComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

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

	// 대상에게서 멀어지는 이동만 후퇴로 본다. 벽·낭떠러지·활동 영역 경계에 막히면 제자리에서 대상을 보며 쿨타임을 기다린다.
	const float MoveDirectionX = FMath::Sign(DestinationX - GetActorLocation().X);
	const bool bRetreating = MoveDirectionX * Side > 0.0f;
	if (bRetreating && !CanRetreatAlongX(MoveDirectionX))
	{
		StopHorizontalMovement();
		FaceLocation(Target->GetActorLocation());
		return;
	}

	MoveAlongX(DestinationX, ChaseSpeed, DeltaSeconds);
	// MoveAlongX는 이동 방향으로 몸을 돌리므로, 후퇴 시에도 대상을 바라보도록 다시 정렬한다.
	FaceLocation(Target->GetActorLocation());
}

bool AKunaiEnemy::CanRetreatAlongX(float DirectionX) const
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const UWorld* World = GetWorld();
	if (!Capsule || !Movement || !World || FMath::IsNearlyZero(DirectionX))
	{
		return false;
	}

	const FVector Location = GetActorLocation();
	const float ProbeDistance = FMath::Max(1.0f, KunaiRetreatProbeDistance);
	const FVector Step = FVector(DirectionX * ProbeDistance, 0.0f, 0.0f);

	// 활동 영역 밖으로 더 멀어지는 후퇴는 막는다. 이미 밖에 있다면 영역 쪽으로 돌아가는 이동은 허용한다.
	const float AreaRadius = FMath::Max(1.0f, CombatAreaRadius);
	const float CurrentOffset = FMath::Abs(Location.X - BehaviorOrigin.X);
	const float NextOffset = FMath::Abs(Location.X + Step.X - BehaviorOrigin.X);
	if (NextOffset > AreaRadius && NextOffset > CurrentOffset)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(KunaiEnemyRetreat), false, this);
	const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
	const FCollisionResponseParams Response(Capsule->GetCollisionResponseToChannels());

	// 벽: 오를 수 있는 계단 높이만큼 바닥을 띄운 캡슐로 후퇴 방향을 쓸어 막히는지 확인한다.
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float StepHeight = FMath::Clamp(Movement->MaxStepHeight, 0.0f, HalfHeight);
	const FVector SweepStart = Location + FVector(0.0f, 0.0f, StepHeight * 0.5f);
	FHitResult WallHit;
	const bool bWallBlocked = World->SweepSingleByChannel(
		WallHit,
		SweepStart,
		SweepStart + Step,
		FQuat::Identity,
		Channel,
		FCollisionShape::MakeCapsule(Radius, FMath::Max(Radius, HalfHeight - StepHeight * 0.5f)),
		Params,
		Response
	);
	if (bWallBlocked)
	{
		return false;
	}

	// 낭떠러지: 후퇴한 위치의 캡슐 앞쪽 아래에 걸을 수 있는 바닥이 있는지 확인한다.
	const FVector GroundStart = Location + Step + FVector(DirectionX * Radius, 0.0f, 0.0f);
	const FVector GroundEnd = GroundStart - FVector(0.0f, 0.0f, HalfHeight + StepHeight + 10.0f);
	FHitResult GroundHit;
	const bool bHasGround = World->LineTraceSingleByChannel(
		GroundHit,
		GroundStart,
		GroundEnd,
		Channel,
		Params,
		Response
	);

	return bHasGround && Movement->IsWalkable(GroundHit);
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
