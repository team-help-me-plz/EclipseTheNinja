
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GABaseAttack.generated.h"

class UForceFeedbackEffect;

/** One click, one montage, and at most one damage application per target. */
UCLASS()
class ECLIPSETHENINJA_API UGABaseAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGABaseAttack();
	static FGameplayTag GetHitEventTag();
	static FGameplayTag GetComboInputEventTag();
	static FGameplayTag GetComboEndEventTag();
	static FGameplayTag GetAttackStartEventTag();
	static FGameplayTag GetAttackingTag();

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
	UPROPERTY(EditDefaultsOnly, Category = "Attack", meta = (ClampMin = "1.0"))
	float AttackRange = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Attack", meta = (ClampMin = "1.0"))
	float AttackRadius = 50.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Attack", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	// 플레이어의 3타가 적에게 적중했을 때 게임패드에 줄 진동. 비워 두면 진동하지 않는다
	UPROPERTY(EditDefaultsOnly, Category = "Attack|ForceFeedback")
	TObjectPtr<UForceFeedbackEffect> ComboFinisherHitForceFeedback;

	// 3타 적중 진동을 플레이어 컨트롤러로 재생한다
	void PlayComboFinisherHitForceFeedback(const ACharacter* InCharacter) const;

	UFUNCTION()
	void OnAttackCompleted();

	UFUNCTION()
	void OnAttackInterrupted();

	UFUNCTION()
	void OnHitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnComboInput(FGameplayEventData Payload);

	UFUNCTION()
	void OnComboCheck(FGameplayEventData Payload);

	UFUNCTION()
	void OnAttackStart(FGameplayEventData Payload);

private:
	TSet<TWeakObjectPtr<AActor>> HitActors;


	// 현재 몇 타인지
	int32 ComboIndex = 1;
	// 다음 입력이 예약되어 있는지
	bool bNextAttackPending = false;
	bool bAcceptComboInput = false;


	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CurrentAttackMontage;
};
