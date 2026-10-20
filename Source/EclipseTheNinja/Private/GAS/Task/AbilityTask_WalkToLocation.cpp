#include "GAS/Task/AbilityTask_WalkToLocation.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UAbilityTask_WalkToLocation::UAbilityTask_WalkToLocation()
{
	bTickingTask = true;
}

UAbilityTask_WalkToLocation* UAbilityTask_WalkToLocation::WalkToLocation(
	UGameplayAbility* OwningAbility, const FVector& Destination, float Timeout, float AcceptanceRadius)
{
	UAbilityTask_WalkToLocation* Task = NewAbilityTask<UAbilityTask_WalkToLocation>(OwningAbility);
	Task->TargetLocation = Destination;
	Task->TimeLimit = FMath::Max(0.05f, Timeout);
	Task->ArrivalRadius = FMath::Square(FMath::Max(1.0f, AcceptanceRadius));
	return Task;
}

void UAbilityTask_WalkToLocation::Activate()
{
	Super::Activate();
	Character = Cast<ACharacter>(GetAvatarActor());
	if (!Character.IsValid())
	{
		FinishWalk(false);
		return;
	}

	SavedMaxWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
	bSpeedSaved = true;
	StartDirection = (TargetLocation - Character->GetActorLocation()).GetSafeNormal2D();
}

void UAbilityTask_WalkToLocation::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);
	if (bFinished) return;

	ACharacter* Walker = Character.Get();
	if (!IsValid(Walker)) { FinishWalk(false); return; }

	UCharacterMovementComponent* Movement = Walker->GetCharacterMovement();
	if (!IsValid(Movement)) { FinishWalk(false); return; }
	if (!Movement->IsMovingOnGround()) { FinishWalk(false); return; }

	const FVector ToTarget = TargetLocation - Walker->GetActorLocation();
	const float DistanceSquared = ToTarget.SizeSquared2D();

	// 도착 반경 안이거나 목적지를 이미 지나쳤으면 도착으로 보고 목적지에 맞춘다.
	// 프레임 시간과 감속에 따라 지나치더라도 왕복하다 시간 초과로 실패하지 않는다.

	// 처음 방향과 지금 현재 타겟까지의 방향의 내적이 음수인지 (반대 방향인지)
	// 반대 방향이면 이미 지나쳤다고 판단
	const bool bPassedTarget = FVector::DotProduct(ToTarget.GetSafeNormal2D(), StartDirection) <= 0.0f;
	if (DistanceSquared <= ArrivalRadius || bPassedTarget)
	{
		FVector SnapLocation = TargetLocation;
		SnapLocation.Z = Walker->GetActorLocation().Z;
		// 벽은 통과하지 않는다. 막혀서 덜 움직이면 어빌리티의 도착 검증에서 취소된다.
		Walker->SetActorLocation(SnapLocation, true);
		FinishWalk(true);
		return;
	}

	ElapsedTime += DeltaTime;
	if (ElapsedTime >= TimeLimit)
	{
		FinishWalk(false);
		return;
	}

	// 도착 직전에 자연스럽게 감속하도록 남은 거리만큼 속도를 제한한다.
	Movement->MaxWalkSpeed = FMath::Min(SavedMaxWalkSpeed, FMath::Sqrt(DistanceSquared) / FMath::Max(DeltaTime, 0.001f));
	Walker->AddMovementInput(ToTarget.GetSafeNormal2D(), 1.0f);
}

void UAbilityTask_WalkToLocation::FinishWalk(bool bReached)
{
	if (bFinished) return;
	bFinished = true;

	if (Character.IsValid())
	{
		Character->ConsumeMovementInputVector();
		Character->GetCharacterMovement()->StopMovementImmediately();
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		if (bReached)
		{
			OnReached.Broadcast();
		}
		else
		{
			OnFailed.Broadcast();
		}
	}

	EndTask();
}

void UAbilityTask_WalkToLocation::OnDestroy(bool bInOwnerFinished)
{
	if (Character.IsValid() && bSpeedSaved)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = SavedMaxWalkSpeed;
		Character->ConsumeMovementInputVector();
		Character->GetCharacterMovement()->StopMovementImmediately();
	}

	Super::OnDestroy(bInOwnerFinished);
}
