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
	const float Distance = ToTarget.SizeSquared2D();
	if (Distance <= ArrivalRadius)
	{
		FinishWalk(true);
		return;
	}

	ElapsedTime += DeltaTime;
	if (ElapsedTime >= TimeLimit)
	{
		FinishWalk(false);
		return;
	}

	// 도착 직전 한 프레임에 목적지를 지나치지 않도록 속도만 제한한다.
	Movement->MaxWalkSpeed = FMath::Min(SavedMaxWalkSpeed, Distance / FMath::Max(DeltaTime, 0.001f));
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
