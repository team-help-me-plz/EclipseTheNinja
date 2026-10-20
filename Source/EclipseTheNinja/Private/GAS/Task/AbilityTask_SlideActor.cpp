#include "GAS/Task/AbilityTask_SlideActor.h"

#include "GameFramework/Actor.h"

UAbilityTask_SlideActor::UAbilityTask_SlideActor()
{
	bTickingTask = true;
}

UAbilityTask_SlideActor* UAbilityTask_SlideActor::SlideActor(UGameplayAbility* OwningAbility, AActor* InActor,
	const FVector& InTargetLocation, float InDuration)
{
	UAbilityTask_SlideActor* Task = NewAbilityTask<UAbilityTask_SlideActor>(OwningAbility);
	Task->Actor = InActor;
	Task->TargetLocation = InTargetLocation;
	Task->Duration = InDuration;
	return Task;
}

void UAbilityTask_SlideActor::Activate()
{
	Super::Activate();

	AActor* Target = Actor.Get();
	if (!IsValid(Target))
	{
		EndTask();
		return;
	}

	StartLocation = Target->GetActorLocation();
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		Target->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
		EndTask();
	}
}

void UAbilityTask_SlideActor::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	AActor* Target = Actor.Get();
	if (!IsValid(Target))
	{
		EndTask();
		return;
	}

	Elapsed += DeltaTime;
	const float Alpha = FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f);
	const FVector Location = FMath::Lerp(StartLocation, TargetLocation, FMath::SmoothStep(0.0f, 1.0f, Alpha));
	Target->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.0f)
	{
		EndTask();
	}
}
