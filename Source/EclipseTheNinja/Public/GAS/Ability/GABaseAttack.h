
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GABaseAttack.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

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

	/** 공격하는 동안 칼에 붙여 재생할 트레일 이펙트 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Effect")
	TObjectPtr<UNiagaraSystem>	SwordTrailSystem = nullptr;

	/**
	 * 트레일을 붙일 칼 메시 소켓. 칼날 중앙에 두고 소켓의 Z축이 칼날 방향을 향해야 한다.
	 * (트레일 리본이 소켓 Z축 방향으로 펼쳐진다)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Effect")
	FName	SwordTrailSocketName = TEXT("b_TrailSocket");

	/** 트레일 리본 폭 (칼날 길이에 맞춘다) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Effect", meta = (ClampMin = "0.0"))
	float	SwordTrailWidth = 60.0f;

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
	// 아바타에 붙은 메시 중 SwordTrailSocketName 소켓을 가진 메시를 찾는다.
	USceneComponent* FindSwordTrailAttachComponent() const;

	TSet<TWeakObjectPtr<AActor>> HitActors;

	// 이번 발동에서 칼에 붙인 트레일 이펙트
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent>	SwordTrailEffect = nullptr;


	// 현재 몇 타인지
	int32 ComboIndex = 1;
	// 다음 입력이 예약되어 있는지
	bool bNextAttackPending = false;
	bool bAcceptComboInput = false;


	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CurrentAttackMontage;
};
