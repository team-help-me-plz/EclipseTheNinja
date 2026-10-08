#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Clone/CloneCommandType.h"
#include "GACloneTech.generated.h"

class ACloneCharacter;
class UAbilityTask_WaitCloneTargeting;
class UInputAction;
class UInputMappingContext;

UCLASS()
class ECLIPSETHENINJA_API UGACloneTech : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGACloneTech();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool	bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void HandleCloneDestroyed(AActor* DestroyedActor);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Target")
	TSubclassOf<ACloneCharacter>	CloneClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|SpawnDist")
	float	CloneDist = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|LifeTime", meta = (ClampMin = "0.01"))
	float	CloneLifeTime = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Targeting")
	TObjectPtr<UInputMappingContext>	TargetingMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Targeting")
	TObjectPtr<UInputAction>	AcceptInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Targeting")
	TObjectPtr<UInputAction>	CancelInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Targeting")
	int32	TargetingMappingPriority = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Targeting", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float	TargetingTimeDilation = 0.2f;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACloneCharacter>	SpawnedClone;

private:
	UFUNCTION()
	void HandleTargetAccepted(const FCloneCommandType& Command);

	UFUNCTION()
	void HandleTargetCancelled();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitCloneTargeting>	TargetingTask;

	bool	bIsSwaped = false;
};
