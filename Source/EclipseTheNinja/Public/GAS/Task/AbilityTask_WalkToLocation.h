#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WalkToLocation.generated.h"

class ACharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWalkToLocationResult);

/** 접근 중에만 일반 이동 입력을 공급해 기존 보행 애니메이션을 사용한다. */
UCLASS()
class ECLIPSETHENINJA_API UAbilityTask_WalkToLocation : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAbilityTask_WalkToLocation();
	
	// 파라미터를 통해서 생성된 태스크를 반환하는 정적 함수
	static UAbilityTask_WalkToLocation* WalkToLocation(UGameplayAbility* OwningAbility,
		const FVector& Destination, float Timeout, float AcceptanceRadius = 5.0f);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

	UPROPERTY(BlueprintAssignable)
	FWalkToLocationResult OnReached;
	UPROPERTY(BlueprintAssignable)
	FWalkToLocationResult OnFailed;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void FinishWalk(bool bReached);
	TWeakObjectPtr<ACharacter> Character;
	FVector TargetLocation = FVector::ZeroVector;
	// 시작 시점의 이동 방향. 목적지를 지나쳤는지 판정하는 기준이다.
	FVector StartDirection = FVector::ZeroVector;
	float TimeLimit = 1.0f;
	float ElapsedTime = 0.0f;
	float ArrivalRadius = 5.0f;
	float SavedMaxWalkSpeed = 0.0f;
	bool bFinished = false;
	bool bSpeedSaved = false;
};
