

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/TimerHandle.h"
#include "GAS/Ability/GACooldownBase.h"
#include "GASwiftStrike.generated.h"

class ACharacter;
class UCurveFloat;
struct FCollisionShape;
struct FCollisionQueryParams;
struct FCollisionResponseParams;

/**
 *
 */
UCLASS()
class ECLIPSETHENINJA_API UGASwiftStrike : public UGACooldownBase
{
	GENERATED_BODY()

public:
	UGASwiftStrike();

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


	static FGameplayTag GetSwiftStrikeAbilityTag();
	// 질풍참 시전 중 상태 태그 (이동·커서 회전 차단용)
	static FGameplayTag GetSwiftStrikingTag();

protected:
	UFUNCTION()
	void OnSwiftStrikeFinished();

	UFUNCTION()
	void OnSwiftStrikeInterrupted();

	UFUNCTION()
	void OnSwiftStrikeMontageCompleted();

private:
	void RestoreDashState();
	// 끝점이 막히는 적과 겹치면 적 뒤 빈 곳으로 옮기고, 없으면 원래 끝점을 쓴다.
	FVector ResolveDashEnd(const FVector& Start, const FVector& End, const FVector& Dir, const FCollisionShape& Shape,
		ECollisionChannel MoveChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& WorldResponse, ECollisionResponse PawnResponse) const;
	void StopDashRootMotion();
	UCurveFloat* GetDashTimeCurve();
	void StartHitStop();
	void EndHitStop();
	void CollectTargets(const ACharacter* Player, const FVector& Start, const FVector& End, const FCollisionShape& Shape);
	void ApplyHitToTargets();
	void DrawDebugHitbox(const FVector& Start, const FVector& End, const FCollisionShape& Shape) const;

protected:
	// 질풍참의 돌진 거리
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike", meta = (ClampMin = "0.0", Units = "cm"))
	float DashDistance = 500.f;

	// 질풍참 시전시간
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike", meta = (ClampMin = "0.0", Units = "s"))
	float Duration = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike", meta = (ClampMin = "0.0"))
	float Damage = 30.f;

	// 돌진 진행 곡선(가로: 정규화 시간, 세로: 정규화 이동 거리). 비어 있으면 DashEaseExponent로 감속 곡선을 만든다.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Feel")
	TObjectPtr<UCurveFloat> DashTimeCurve;

	// 클수록 출발 직후 대부분을 이동하고 끝에서 급하게 멈춘다. 1이면 등속.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Feel", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float DashEaseExponent = 3.f;

	// 돌진이 끝난 뒤 남기는 속도. 0이면 그 자리에서 멈추고, 값이 있으면 미끄러지며 선다.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Feel", meta = (ClampMin = "0.0", Units = "cm/s"))
	float DashEndSpeed = 300.f;

	// 끝점이 적과 겹칠 때 적을 관통해 더 나아갈 수 있는 최대 거리
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Collision", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxPassThroughDistance = 150.f;

	// 겹치지 않는 끝점을 찾을 때 한 번에 옮기는 거리
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Collision", meta = (ClampMin = "1.0", Units = "cm"))
	float EndSearchStep = 10.f;

	// 적을 맞혔을 때 플레이어와 맞은 적을 멈추는 시간(실제 시간). 0이면 끈다.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Feel", meta = (ClampMin = "0.0", Units = "s"))
	float HitStopDuration = 0.07f;

	// 히트스톱 동안의 시간 배속
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Feel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HitStopTimeDilation = 0.05f;

	// 질풍참 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike")
	TSoftObjectPtr<UAnimMontage> SwiftStrikeMontage;

	// 공중에서 시전하면 지상 베기 대신 전용 전신 돌진 포즈를 사용한다.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike")
	TSoftObjectPtr<UAnimMontage> AirSwiftStrikeMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Effect")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// 돌진 판정 캡슐의 시작·끝과 쓸고 지나간 경로, 맞은 적을 그린다
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Debug")
	bool bDrawDebugHitbox = false;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Debug", meta = (EditCondition = "bDrawDebugHitbox", ClampMin = "0.0", Units = "s"))
	float DebugDrawTime = 2.f;


private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> LoadedSwiftStrikeMontage;

	TArray<FHitResult> AttackTargets;

	// DashTimeCurve가 비어 있을 때 쓰는 감속 곡선
	UPROPERTY(Transient)
	TObjectPtr<UCurveFloat> GeneratedDashTimeCurve;
	float GeneratedDashEaseExponent = 0.f;

	uint16 DashRootMotionSourceID = 0;
	FVector DashDirection = FVector::ZeroVector;
	FTimerHandle DashTimerHandle;

	FTimerHandle HitStopTimerHandle;
	TArray<TWeakObjectPtr<AActor>> HitStopActors;

	bool bDashFinished = false;
	bool bMontageFinished = true;
	bool bUsingAirMontage = false;
	bool bDashInvincibilityAdded = false;
	bool bDashCollisionChanged = false;
	ECollisionResponse SavedPawnCollisionResponse = ECR_Block;


};
