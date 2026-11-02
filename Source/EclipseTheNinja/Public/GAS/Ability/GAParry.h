#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GAParry.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;

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
    static bool TryParry(AActor* Defender, AActor* Attacker, const FHitResult& Hit, bool bCanBeParried);

protected:

    /** 패링 몽타주를 지정하지 않았을 때만 사용하는 판정 시간. */
    UPROPERTY(EditDefaultsOnly, Category = "Parry", meta = (ClampMin = "0.01", EditCondition = "ParryMontage == nullptr"))
    float WindowDuration = 0.18f;

    UPROPERTY(EditDefaultsOnly, Category = "Parry", meta = (ClampMin = "0.01"))
    float RecoveryDuration = 0.3f;

    UPROPERTY(EditDefaultsOnly, Category = "Parry", meta = (ClampMin = "0.01"))
    float AttackerStunDuration = 0.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Parry|Animation")
    TObjectPtr<UAnimMontage> ParryMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Parry|Animation")
    TObjectPtr<UAnimMontage> SuccessMontage;
    UFUNCTION(BlueprintImplementableEvent, Category = "Parry")
    void OnParrySucceeded(AActor* Attacker);

private:
    UFUNCTION()
    void CloseWindow();
    UFUNCTION()
    void StartRecovery();
    UFUNCTION()
    void FinishRecovery();
    UFUNCTION()
    void HandleSuccess(FGameplayEventData Payload);
    UFUNCTION()
    void HandleMontageInterrupted();

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_WaitDelay> WindowTask;

    bool bWindowOpen = false;
    bool bSucceeded = false;
    bool bRecoveryStarted = false;
};
