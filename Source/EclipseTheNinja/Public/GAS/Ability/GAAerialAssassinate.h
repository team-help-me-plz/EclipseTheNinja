#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GAAssassinateBase.h"
#include "GAAerialAssassinate.generated.h"

/**
 * 적보다 높은 발판 위에서 뛰어내려 내려찍는 공중 암살.
 * 페어 애니메이션이므로 암살자·피격자 몽타주를 동시에 시작하고, 암살자 몽타주의 루트가 움직이기 시작하는 시점부터
 * 착지 시점까지 적 뒤 착지 위치로 포물선 점프한다. 발판 높이가 달라도 맞도록 이동은 점프가 맡고 몽타주의 루트 모션은 끈다. */
UCLASS()
class ECLIPSETHENINJA_API UGAAerialAssassinate : public UGAAssassinateBase
{
	GENERATED_BODY()

protected:
	virtual bool CanAssassinateTarget(const APlayerCharacter* Player, const UAssassinateTargetComponent* Target,
		const AEnemy* Enemy) const override;
	virtual void StartApproach() override;

	/** 켜면 몽타주 루트 모션으로 이동 시작(루트가 처음 움직이는 때)·착지 시점을 찾는다. 찾지 못하면 아래 값을 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination")
	bool bDetectJumpFromRootMotion = true;

	/** 몽타주 시작부터 이동을 시작하는 시점까지의 시간. 그 전에는 제자리에서 준비 동작만 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination", meta = (ClampMin = "0.0", Units = "s"))
	float JumpStartTime = 0.45f;

	/** 이동을 시작한 뒤 착지까지 걸리는 시간. 몽타주의 착지 시점에 맞춘다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination", meta = (ClampMin = "0.05", Units = "s"))
	float JumpDuration = 0.33f;

	/**
	 * 칼날이 피격자 몸통을 지나도록 피격자를 옆으로 옮길 거리(cm). 암살자 기준 오른쪽이 +, 왼쪽이 -다.
	 * 횡스크롤에서는 화면 깊이 방향(월드 Y축)이다. 칼날과 몸통 위치로 애니메이션에서 구한 값이다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination", meta = (Units = "cm"))
	float VictimSideOffset = -23.0f;

private:
	// 암살을 위한 점프 시작
	UFUNCTION()
	void StartJump();

	// 몽타주 에서 착지할 때 호출되는 함수
	UFUNCTION()
	void OnJumpLanded();

	// 이동 시작·착지 시점을 정한다. 몽타주 루트 모션에서 찾거나 설정값을 쓴다.
	void ResolveJumpWindow();

	FVector LandingLocation = FVector::ZeroVector;
	float ResolvedJumpStartTime = 0.0f;
	float ResolvedJumpDuration = 0.0f;
};
