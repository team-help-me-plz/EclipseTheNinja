

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

	// bDrawDebugClimb가 켜져 있으면 지금 위치에서 올라가기 판정을 LifeTime 동안 그린다. 매달리기(GAWallAttach)가 매달린 동안 계속 부른다.
	void DrawClimbDebug(const ACharacter* Player, float LifeTime) const;

protected:
	// 매달린 위치에서 올라설 위치를 찾는다. 바라보는 방향(벽 쪽)의 모서리만 본다.
	// DebugLifeTime이 0 이상이면 판정 과정을 그 시간 동안 그린다.
	bool FindClimb(const ACharacter* Player, FVector& OutStandLocation, float& OutFacing, float DebugLifeTime = -1.f) const;

	// PathCurve 경로가 모서리를 뚫지 않게 높이를 보정한다. 캡슐이 벽 안쪽으로 들어가는 만큼 둥근 바닥이 윗면 위에 있어야 하고,
	// 필요한 높이까지 미리(앞 구간으로) 서서히 올라가게 한다. 몽타주 루트 모션은 루트가 모서리를 지나가므로 그대로 따라가면 걸려 튄다.
	void ClearLedgeInPath(const ACharacter* Player, const FVector& Start, float LedgeTopZ);

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

	// 올라갈 벽을 찾을 때 캡슐 표면 앞으로 더 보는 거리 (캡슐 표면 기준) 약갼의 여유를줌.
	UPROPERTY(EditDefaultsOnly, Category = "LedgeClimb", meta = (ClampMin = "0.0", Units = "cm"))
	float WallTraceMargin = 30.f;

	// 몽타주나 루트 모션이 없을 때 올라가는 시간
	UPROPERTY(EditDefaultsOnly, Category = "LedgeClimb", meta = (ClampMin = "0.05", Units = "s"))
	float ClimbTime = 0.5f;

	// 올라가기 몽타주. 루트 모션이 있으면 그 궤적을 따라 이동하고, 몽타주 자체의 루트 모션은 끈다.
	UPROPERTY(EditDefaultsOnly, Category = "LedgeClimb")
	TSoftObjectPtr<UAnimMontage> ClimbMontage;

	// 매달려 있는 동안 올라가기 가능 범위와 판정(벽, 모서리 윗면, 설 자리, 올라가는 경로)을 계속 그린다
	UPROPERTY(EditDefaultsOnly, Category = "LedgeClimb|Debug")
	bool bDrawDebugClimb = false;

private:
	FVector StandLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> LoadedClimbMontage;

	UPROPERTY(Transient)
	TObjectPtr<UCurveVector> PathCurve;

	bool bMovementChanged = false;
	bool bReachedStand = false;
};
