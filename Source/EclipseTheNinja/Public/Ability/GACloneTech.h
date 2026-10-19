#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Clone/CloneCommandType.h"
#include "GACloneTech.generated.h"

class ACloneCharacter;
class UAbilityTask_WaitCloneTargeting;
class UInputAction;
class UInputMappingContext;
class UNiagaraSystem;

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

	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// 분신이 풀에 반환되거나 파괴되면 어빌리티를 종료한다
	UFUNCTION()
	void HandleCloneDespawned(ACloneCharacter* DespawnedClone);

	void	OnRecallClone();

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
	TObjectPtr<UNiagaraSystem>		DestinationSystem = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Targeting")
	int32	TargetingMappingPriority = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Targeting", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float	TargetingTimeDilation = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Despawn")
	float	RecallCooldown = 1.f;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACloneCharacter>	SpawnedClone;

private:
	UFUNCTION()
	void HandleTargetAccepted(const FCloneCommandType& Command);

	UFUNCTION()
	void HandleTargetCancelled();

	/** 플레이어 캐릭터에 분신술 타게팅 상태를 알린다. */
	void SetAvatarCloneTargeting(bool bTargeting) const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitCloneTargeting>	TargetingTask;

	bool	bIsSwaped = false;

	mutable FActiveGameplayEffectHandle	CooldownEffectHandle;
};
