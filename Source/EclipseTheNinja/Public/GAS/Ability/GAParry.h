#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GAParry.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;
class UNiagaraSystem;
class AEnemy;

/** 패링 몽타주 재생 중 판정 창을 유지하고, 종료 후 후딜을 관리한다. */
UCLASS()
class ECLIPSETHENINJA_API UGAParry : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGAParry();
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    // 어빌리티를 받을 때 기본 불꽃을 보이지 않게 한 번 재생해 둔다. 첫 패링 성공 때 이펙트 생성으로 멈추지 않게 한다.
    virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

    /**
     * 플레이어 ASC의 GE 적용 질의에서 호출한다. 판정 창이 열려 있을 때 들어온 공격을 원인에 따라 처리한다.
     * - EffectCauser가 IParryDeflectable(투사체)이면 튕겨낸다.
     * - Instigator가 적이면 그 적에게 패링 피격 경직을 준다.
     * 패링으로 처리했으면 true를 반환하며, 이때 피해 GE는 적용되지 않아야 한다.
     */
    static bool TryParryIncomingEffect(UAbilitySystemComponent* DefenderASC, const FGameplayEffectSpec& Spec);

    // ASC에서 실행 중인 패링 인스턴스를 찾는다. 없으면 nullptr.
    static UGAParry* FindActiveParry(UAbilitySystemComponent* ASC);

    // Parry Window 노티파이 구간의 시작/끝에서 호출한다. 패링 몽타주에 노티파이가 있을 때만 판정을 연다.
    void OpenWindowFromNotify();
    void CloseWindowFromNotify();

protected:

    /** 패링 몽타주를 지정하지 않았을 때만 사용하는 판정 시간. */
    UPROPERTY(EditDefaultsOnly, Category = "Parry", meta = (ClampMin = "0.01", EditCondition = "ParryMontage == nullptr"))
    float WindowDuration = 0.18f;

    UPROPERTY(EditDefaultsOnly, Category = "Parry", meta = (ClampMin = "0.01"))
    float RecoveryDuration = 0.3f;

    /** 적에게 패링 피격 몽타주가 없을 때 적용할 경직 시간. 몽타주가 있으면 그 길이를 따른다. */
    UPROPERTY(EditDefaultsOnly, Category = "Parry", meta = (ClampMin = "0.01"))
    float AttackerStunDuration = 0.5f;

    /** 판정 구간은 몽타주(또는 그 안의 시퀀스)에 넣은 Parry Window 노티파이를 따른다. 노티파이가 없으면 몽타주 전체가 판정 구간이다. */
    UPROPERTY(EditDefaultsOnly, Category = "Parry|Animation")
    TObjectPtr<UAnimMontage> ParryMontage;

    /**
     * 적의 공격을 막았을 때 재생할 동작. 투사체를 튕겨낼 때는 재생하지 않고 패링 동작을 이어 간다.
     * 슬로우 모션이 필요하면 이 몽타주에 Slow Motion 노티파이를 넣는다.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Parry|Animation")
    TObjectPtr<UAnimMontage> SuccessMontage;
    /**
     * 적의 공격을 막았을 때 부딪힌 자리에 재생할 기본 불꽃. 투사체를 튕겨낼 때는 재생하지 않는다.
     * 막은 적에 ParrySparkEffect가 지정돼 있으면(예: 보스) 그 적의 불꽃이 우선한다. 둘 다 비우면 재생하지 않는다.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Parry|Effect")
    TObjectPtr<UNiagaraSystem> SuccessSparkEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Parry|Effect", meta = (ClampMin = "0.01"))
    float SuccessSparkScale = 1.0f;

    /** 불꽃 위치. 오른손에서 바라보는 방향으로 이만큼 앞(X), 위(Y)로 띄운다. */
    UPROPERTY(EditDefaultsOnly, Category = "Parry|Effect", meta = (Units = "cm"))
    FVector2D SuccessSparkHandOffset = FVector2D(40.0f, 0.0f);

    UFUNCTION(BlueprintImplementableEvent, Category = "Parry")
    void OnParrySucceeded(AActor* Attacker);

private:
    void OpenWindow();
    UFUNCTION()
    void CloseWindow();
    // 몽타주 자체 또는 구간 시퀀스에 Parry Window 노티파이가 있는지.
    static bool HasParryWindowNotify(const UAnimMontage* Montage);
    UFUNCTION()
    void StartRecovery();
    UFUNCTION()
    void FinishRecovery();
    // 공격자/원인을 판별해 패링을 적용한다. 처리했으면 true.
    bool ResolveIncomingAttack(AActor* Attacker, AActor* Causer);
    // 판정을 닫고 불꽃, 큐, BP 이벤트를 재생한다. 적의 공격을 막았으면(투사체가 아니면) 성공 동작도 재생한다.
    void HandleSuccess(AActor* Attacker, AActor* DeflectedProjectile);
    void SpawnSuccessSpark(const AEnemy* Attacker) const;
    UFUNCTION()
    void HandleMontageInterrupted();

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_WaitDelay> WindowTask;

    bool bWindowOpen = false;
    // 이번 패링이 노티파이 구간으로 판정하는지. false면 몽타주 시작부터 끝까지 판정한다.
    bool bWindowByNotify = false;
    bool bSucceeded = false;
    bool bRecoveryStarted = false;
};
