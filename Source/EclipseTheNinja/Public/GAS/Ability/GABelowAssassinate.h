#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GAAssassinateBase.h"
#include "GABelowAssassinate.generated.h"

/**
 * 적이 선 발판보다 낮은 바닥에서 모서리를 잡고 올라가 목을 꺾는 아래 암살.
 * 암살자 몽타주는 [올라가기 구간 + 목 꺾기 구간]으로 되어 있다.
 * 올라가기와 잡으러 다가가는 동안은 몽타주 루트 모션의 진행 비율을 따라 코드가 이동시키고(발판 높이·거리에 맞춤),
 * 적을 잡은 뒤에는 몽타주 루트 모션을 켜서 뒤로 물러나는 걸음을 그대로 쓴다.
 * 피격자 몽타주는 목 꺾기 구간이 시작될 때 함께 시작해야 맞물린다.
 * 두 구간 경계의 자세 차이는 몽타주를 목 꺾기 구간부터 다시 재생해 Blend In 시간만큼 섞어 넘긴다.
 */
UCLASS()
class ECLIPSETHENINJA_API UGABelowAssassinate : public UGAAssassinateBase
{
	GENERATED_BODY()

protected:
	virtual bool CanAssassinateTarget(const APlayerCharacter* Player, const UAssassinateTargetComponent* Target,
		const AEnemy* Enemy) const override;
	virtual void StartApproach() override;

	/** 켜면 몽타주의 두 번째 구간이 시작하는 시점을 올라가기가 끝나는 시점으로 쓴다. 구간이 하나뿐이면 아래 값을 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination")
	bool bDetectClimbEndFromMontage = true;

	/** 몽타주 시작부터 올라가기가 끝나는(목 꺾기 구간이 시작되는) 시점. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination", meta = (ClampMin = "0.05", Units = "s"))
	float ClimbEndTime = 0.687f;

	/** 목 꺾기 구간이 시작된 뒤 적의 목을 잡는 시점까지의 시간. 이때까지 적 뒤로 다가간다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination", meta = (ClampMin = "0.05", Units = "s"))
	float GrabTime = 0.6f;

	/**
	 * 목을 잡는 동안 손이 피격자 목에 오도록 피격자를 옆으로 옮길 거리(cm). 암살자 기준 오른쪽이 +, 왼쪽이 -다.
	 * 횡스크롤에서는 화면 깊이 방향(월드 Y축)이다. 손과 목 위치로 애니메이션에서 구한 값이다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination", meta = (Units = "cm"))
	float VictimSideOffset = -34.0f;

private:
	UFUNCTION()
	void OnClimbFinished();

	// 올라가기 구간이 끝나기 직전에 목 꺾기 구간을 새로 재생해 두 자세를 섞는다.
	UFUNCTION()
	void OnSectionCrossfade();

	UFUNCTION()
	void OnGrabReached();

	// 올라가기가 끝나는 시점을 정한다. 몽타주 구간에서 찾거나 설정값을 쓴다.
	void ResolveClimbEndTime();

	FVector ClimbLocation = FVector::ZeroVector;
	FVector GrabLocation = FVector::ZeroVector;
	// 적이 모서리에 가까워 잡는 거리가 모자랄 때 피격자를 앞으로 밀 거리
	float VictimForwardPush = 0.0f;
	float ResolvedClimbEndTime = 0.0f;
	// 구간 경계를 섞는 시간. 몽타주의 Blend In 시간을 쓴다.
	float CrossfadeTime = 0.0f;
};
