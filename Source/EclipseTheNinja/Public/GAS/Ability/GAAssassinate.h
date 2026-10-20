

#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GAAssassinateBase.h"
#include "GAAssassinate.generated.h"

/**
 * 평지 후방 암살. 적 등 뒤 위치까지 걸어가 실행 몽타주를 재생한다.
 */
UCLASS()
class ECLIPSETHENINJA_API UGAAssassinate : public UGAAssassinateBase
{
	GENERATED_BODY()

protected:
	// 해당 타겟을 암살할 수 있는지 반환하는 함수
	virtual bool CanAssassinateTarget(const APlayerCharacter* Player, const UAssassinateTargetComponent* Target,
		const AEnemy* Enemy) const override;
	// 접근 시작하는 함수
	virtual void StartApproach() override;

	// 접근 시간
	UPROPERTY(EditDefaultsOnly, Category = "Assassination", meta = (ClampMin = "0.05", Units = "s"))
	float ApproachTimeout = 1.0f;

private:
	// 암살을 하기위해서 접근이 끝났을 떄 호출될 함수
	UFUNCTION()
	void OnApproachCompleted();

	FVector ApproachLocation = FVector::ZeroVector;
};
