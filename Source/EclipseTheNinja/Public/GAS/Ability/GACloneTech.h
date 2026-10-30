#pragma once

#include "CoreMinimal.h"
#include "Clone/CloneCommandType.h"

#include "GAS/Ability/GACooldownBase.h"
#include "GACloneTech.generated.h"

class ACloneCharacter;
class UAbilityTask_WaitCloneTargeting;
class UInputAction;
class UInputMappingContext;
class UNiagaraSystem;

// 분신이 소환됐을 때 델리게이트
DECLARE_MULTICAST_DELEGATE(FOnCloneSpawned);
// 분신과 플레이어가 더 이상 Swap할 수 없을 때 델리게이트
DECLARE_MULTICAST_DELEGATE(FOnCloneCannotSwap);

UCLASS()
class ECLIPSETHENINJA_API UGACloneTech : public UGACooldownBase
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

	// 분신이 풀에 반환되거나 파괴되면 어빌리티를 종료한다
	UFUNCTION()
	void HandleCloneDespawned(ACloneCharacter* DespawnedClone);

	void	OnRecallClone();

	FOnCloneSpawned		OnCloneSpawned;
	FOnCloneCannotSwap	OnCloneCannotSwap;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Swap")
	float	SwapMargin = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Despawn")
	float	RecallCooldown = 1.f;

	// 분신이 소환되거나 사라질 때(회수, 수명 종료) 분신 위치에서 재생할 이펙트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Effect")
	TObjectPtr<UNiagaraSystem>	CloneEffectSystem = nullptr;

	// 이펙트를 재생할 시간. 루프 이펙트라서 이 시간이 지나면 새 파티클 생성을 멈춘다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Effect", meta = (ClampMin = "0.01"))
	float	CloneEffectDuration = 0.3f;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACloneCharacter>	SpawnedClone;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Despawn")
	FText	CannotSwapText = FText::FromString(TEXT("위치를 교환할 수 없습니다."));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Spawn")
	FText	CannotSpawnText = FText::FromString(TEXT("분신을 소환할 수 없는 위치입니다."));

private:
	UFUNCTION()
	void HandleTargetAccepted(const FCloneCommandType& Command);

	UFUNCTION()
	void HandleTargetCancelled();

	/** 플레이어 캐릭터에 분신술 타게팅 상태를 알린다. */
	void SetAvatarCloneTargeting(bool bTargeting) const;

	/** 분신 소환/회수 이펙트를 InLocation에서 CloneEffectDuration 동안 재생한다. */
	void PlayCloneEffect(const FVector& InLocation) const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitCloneTargeting>	TargetingTask;

	bool	bIsSwaped = false;

};
