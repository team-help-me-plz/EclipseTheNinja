

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GAS/Ability/GACooldownBase.h"
#include "GASwiftStrike.generated.h"

class ACharacter;
struct FCollisionShape;

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

	bool bDashFinished = false;
	bool bMontageFinished = true;
	bool bUsingAirMontage = false;
	bool bDashInvincibilityAdded = false;
	bool bDashCollisionChanged = false;
	ECollisionResponse SavedPawnCollisionResponse = ECR_Block;


};
