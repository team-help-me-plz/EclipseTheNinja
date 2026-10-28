

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GASwiftStrike.generated.h"

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UGASwiftStrike : public UGameplayAbility
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

protected:
	// 질풍참의 돌진 거리
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike", meta = (ClampMin = "0.0", Units = "cm"))
	float DashDistance = 500.f;

	// 질풍참 시전시간
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike", meta = (ClampMin = "0.0", Units = "s"))
	float Duration = 0.12f;

	// 질풍참 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike")
	TSoftObjectPtr<UAnimMontage> SwiftStrikeMontage;


private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> LoadedSwiftStrikeMontage;


};
