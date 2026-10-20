


#include "GAS/Ability/GAAssassinate.h"
#include "GAS/Task/AbilityTask_WalkToLocation.h"
#include "Character/Enemy.h"
#include "Character/PlayerCharacter.h"
#include "Component/AssassinateTargetComponent.h"

bool UGAAssassinate::CanAssassinateTarget(const APlayerCharacter* Player, const UAssassinateTargetComponent* Target,
	const AEnemy* Enemy) const
{
	if (!Player->GetCharacterMovement()->IsMovingOnGround()) return false;
	return Target->IsApproachClear(Player);
}

void UGAAssassinate::StartApproach()
{
	// 암살자와 암살대상을 가져오고 유효성 검사를 한다.
	APlayerCharacter* Player = Assassin.Get();
	AEnemy* Enemy = Victim.Get();
	if (!IsValid(Player) || !IsValid(Enemy) || !TargetComponent.IsValid()) { OnInterrupted(); return; }

	// 타겟팅의 벽 검사와 같은 위치를 쓰도록 등 뒤 위치는 대상 컴포넌트에서 받는다.
	ApproachLocation = TargetComponent->GetApproachLocation(Player);

	// 이미 가까우면 뒤로 걷지 않고 연출 간격만 즉시 맞춘다. 벽은 통과하지 않는다.
	if (FVector::DistSquared2D(Player->GetActorLocation(), Enemy->GetActorLocation())
		<= FVector::DistSquared2D(ApproachLocation, Enemy->GetActorLocation()))
	{
		Player->SetActorLocation(ApproachLocation, true);
		if (FVector::DistSquared2D(Player->GetActorLocation(), ApproachLocation) > FMath::Square(1.0f))
		{
			OnInterrupted();
			return;
		}

		OnApproachCompleted();
		return;
	}

	// 위의 경우가 아니더라도 적절한 거리 내에 있다면 도착 완료 처리
	if (FVector::DistSquared2D(Player->GetActorLocation(), ApproachLocation) <= FMath::Square(10.0f))
	{
		OnApproachCompleted();
		return;
	}

	// 아닌 경우 암살 위치까지 움직이는 태스크를 준비
	UAbilityTask_WalkToLocation* MoveTask = UAbilityTask_WalkToLocation::WalkToLocation(
		this, ApproachLocation, ApproachTimeout);
	MoveTask->OnReached.AddDynamic(this, &UGAAssassinate::OnApproachCompleted);
	MoveTask->OnFailed.AddDynamic(this, &UGAAssassinate::OnInterrupted);
	MoveTask->ReadyForActivation();
}

void UGAAssassinate::OnApproachCompleted()
{
	if (bEnding) return;
	if (!IsActive()) return;

	APlayerCharacter* Player = Assassin.Get();
	if (!IsValid(Player)) { OnInterrupted(); return; }

	// 이동 태스크의 도착 판정보다 엄격히 확인해 벽에 막힌 채 처치하지 않는다.
	if (FVector::DistSquared2D(Player->GetActorLocation(), ApproachLocation) > FMath::Square(20.f))
	{
		OnInterrupted();
		return;
	}

	// 암살자의 움직임을 잠그고 암살 수행
	Player->GetCharacterMovement()->StopMovementImmediately();
	Player->GetCharacterMovement()->DisableMovement();
	PlayExecution();
}
