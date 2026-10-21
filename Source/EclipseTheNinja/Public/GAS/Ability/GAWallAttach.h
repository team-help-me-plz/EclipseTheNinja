

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GAWallAttach.generated.h"

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UGAWallAttach : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGAWallAttach();

	static FGameplayTag GetWallAttachingTag();

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
	UFUNCTION()
	void OnCompleted();

	UFUNCTION()
	void OnInterrupted();

private:
	bool FindWall(const ACharacter* Player) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach")
	bool bCanAttachOnGround = false;

	// 벽을 찾을 거리
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach", meta = (ClampMin = "0.0", Units = "cm"))
	float WallSearchDist = 50.f;

	// 벽을 찾을때 사용될 캡슐의 반지름
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach", meta = (ClampMin = "0.0", Units = "cm"))
	float WallSearchRadius = 10.f;

	// 벽을 찾을때 사용될 캡슐의 높이
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach", meta = (ClampMin = "0.0", Units = "cm"))
	float WallSearchHalfHeight = 35.f;
};
