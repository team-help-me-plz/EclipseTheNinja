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
	virtual EAssassinationType GetAssassinationType() const override { return EAssassinationType::Below; }
	virtual const TSoftObjectPtr<UAnimMontage>& ResolveAttackMontage(const AEnemy* Enemy) override;
	virtual void AddPreloadPaths(TArray<FSoftObjectPath>& OutPaths) const override;
	virtual bool IsBareHandedExecution() const override { return !bUsingBossStab; }

	/**
	 * 피격자가 보스일 때 쓰는 암살자 몽타주. [올라가기 구간 + 칼 찌르기 구간]으로 되어 있다.
	 * 보스는 키와 캡슐이 커서 목을 꺾을 수 없으므로, 올라간 뒤 캡슐이 겹치지 않는 거리에서 등을 찌른다.
	 * 비우면 보스에게도 AttackMontage(목 꺾기)를 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Boss")
	TSoftObjectPtr<UAnimMontage> BossAttackMontage;

	/** 찌르기 구간이 시작된 뒤 칼이 꽂히는 시점까지의 시간. 이때까지 보스 뒤 찌르는 자리로 다가간다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Boss", meta = (ClampMin = "0.05", Units = "s"))
	float BossGrabTime = 0.812f;

	/** 칼이 꽂히는 순간 암살자와 보스 원점 사이 거리(cm). 캡슐이 겹치는 거리보다 가까우면 캡슐 밖에 선다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Boss", meta = (ClampMin = "0.0", Units = "cm"))
	float BossStabDistance = 63.0f;

	/** 찌르는 순간 칼날이 보스 몸통을 지나도록 보스를 옆으로 옮길 거리(cm). 암살자 기준 오른쪽이 +다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination|Boss", meta = (Units = "cm"))
	float BossVictimSideOffset = 0.0f;

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
	// 이번 암살이 보스 찌르기 몽타주를 쓰는지와, 그에 맞춘 잡는(찌르는) 시점·옆 이동 거리
	bool bUsingBossStab = false;
	float ResolvedGrabTime = 0.0f;
	float ResolvedVictimSideOffset = 0.0f;
	// 구간 경계를 섞는 시간. 몽타주의 Blend In 시간을 쓴다.
	float CrossfadeTime = 0.0f;
};
