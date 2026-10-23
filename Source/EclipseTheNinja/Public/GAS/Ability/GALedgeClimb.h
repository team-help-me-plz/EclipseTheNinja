

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GALedgeClimb.generated.h"

class UAnimMontage;
class UCurveVector;

/**
 * 벽에 매달린 상태에서 손이 닿는 곳에 모서리가 있으면 위로 올라선다.
 * 매달리기(GAWallAttach)를 취소하고, 올라가기 몽타주의 루트 모션 궤적을 따라 모서리 위 설 자리까지 코드가 이동시킨다.
 * 하단 암살의 올라가기 구간과 같은 방식이며, 모서리 매달리기(GALedgeHang)의 내려가기를 정방향으로 재생한 동작이다.
 */
UCLASS()
class ECLIPSETHENINJA_API UGALedgeClimb : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGALedgeClimb();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 매달린 위치에서 올라설 위치를 찾는다. 바라보는 방향(벽 쪽)의 모서리만 본다.
	bool FindClimb(const ACharacter* Player, FVector& OutStandLocation, float& OutFacing) const;

	UFUNCTION()
	void OnClimbFinished();

	UFUNCTION()
	void OnInterrupted();

	// 매달린 캡슐 중심에서 모서리 윗면까지 이 높이 안에 있어야 올라갈 수 있다
	UPROPERTY(EditDefaultsOnly, Category = "LedgeClimb", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxReachHeight = 200.f;

	// 모서리 안쪽으로 이만큼 들어가서 선다 (캡슐 표면 기준)
	UPROPERTY(EditDefaultsOnly, Category = "LedgeClimb", meta = (ClampMin = "0.0", Units = "cm"))
	float ClimbInset = 20.f;

	// 몽타주나 루트 모션이 없을 때 올라가는 시간
	UPROPERTY(EditDefaultsOnly, Category = "LedgeClimb", meta = (ClampMin = "0.05", Units = "s"))
	float ClimbTime = 0.5f;

	// 올라가기 몽타주. 루트 모션이 있으면 그 궤적을 따라 이동하고, 몽타주 자체의 루트 모션은 끈다.
	UPROPERTY(EditDefaultsOnly, Category = "LedgeClimb")
	TSoftObjectPtr<UAnimMontage> ClimbMontage;

private:
	FVector StandLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> LoadedClimbMontage;

	UPROPERTY(Transient)
	TObjectPtr<UCurveVector> PathCurve;

	bool bMovementChanged = false;
	bool bReachedStand = false;
};
