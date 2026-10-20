#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_SlideActor.generated.h"

/**
 * 어빌리티 소유자가 아닌 다른 액터를 지정한 위치까지 정해진 시간 동안 부드럽게 옮긴다.
 * 이동이 꺼진 피격자를 페어 애니메이션 위치에 맞출 때 쓴다. 충돌은 검사하지 않는다.
 */
UCLASS()
class ECLIPSETHENINJA_API UAbilityTask_SlideActor : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAbilityTask_SlideActor();

	static UAbilityTask_SlideActor* SlideActor(UGameplayAbility* OwningAbility, AActor* Actor,
		const FVector& TargetLocation, float Duration);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

private:
	TWeakObjectPtr<AActor> Actor;
	FVector StartLocation = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;
	float Duration = 0.2f;
	float Elapsed = 0.0f;
};
