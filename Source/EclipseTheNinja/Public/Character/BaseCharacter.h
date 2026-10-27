#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GAS/AbilityInfo.h"
#include "BaseCharacter.generated.h"

class UAbilitySystemComponent;
class UStatAttributeSet;
class UAnimMontage;
class UGABaseAttack;
class UGameplayEffect;
struct FGameplayAbilitySpec;

/** Player와 Enemy가 공유하는 이동 설정, GAS, 전투 데이터의 부모 클래스. */
UCLASS(Abstract)
class ECLIPSETHENINJA_API ABaseCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ABaseCharacter();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    UStatAttributeSet* GetStatAttribute() const;
    UAnimMontage* GetBasicAttackMontage() const { return BasicAttackMontage; }

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void DoBasicAttack();

    /** 어빌리티 배열에서 해당 입력 ID로 부여된 어빌리티 스펙을 찾는다. */
    FGameplayAbilitySpec* FindAbilitySpecByInputID(EAbilityInputID InputID) const;

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
    virtual void PostLoad() override;
    void InitializeAbilitySystem();

    // 시작 스탯 GE를 한 번만 적용한다. 재빙의 시 다시 적용해 체력이 회복되지 않도록 한다.
    void ApplyDefaultStats();

    // 어빌리티 배열에 지정된 어빌리티를 부여하고, 이미 있으면 입력 ID와 레벨만 갱신한다.
    void GiveInitAbility();

    // 입력 ID로 부여된 어빌리티를 발동한다. 콤보 입력처럼 InputPressed 이벤트는 보내지 않는다.
    bool TryActivateAbilityByInputID(EAbilityInputID InputID);

    // 필수 어빌리티가 배열에 빠져 있으면 경고를 남긴다.
    void WarnIfAbilityMissing(EAbilityInputID InputID) const;

    // 클래스 프로퍼티로 부여하던 예전 BP 설정을 어빌리티 배열로 옮긴다.
    virtual void MigrateLegacyAbilities();
    void MigrateLegacyAbility(EAbilityInputID InputID, TSubclassOf<UGameplayAbility> LegacyClass,
        TSubclassOf<UGameplayAbility> DefaultClass);

    // 피격 몽타주 재생용 함수
    UFUNCTION(BlueprintCallable, Category = "Combat|Hit Reaction")
    virtual void PlayHitReaction();

    // 몽타주 블랜드아웃용 함수
    void OnDeathMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

private:
    UAnimMontage* GetBasicAttackMontage(const ACharacter* Character) const;


protected:
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Death")
    bool bIsDead = false;

    // 기존 BP 설정을 유지하도록 컴포넌트와 프로퍼티 이름을 보존한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<UAbilitySystemComponent> ASC;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<UStatAttributeSet> StatAttributeSet;

    /** 시작 시 스탯을 덮어쓸 Instant GE. 캐릭터 종류마다 다른 GE를 지정한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
    TSubclassOf<UGameplayEffect> DefaultStatsEffect;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UAnimMontage> BasicAttackMontage;

    /** 시작 시 부여할 어빌리티와 발동에 쓸 입력 ID. 기본 공격 등 모든 기본 어빌리티를 여기서 지정한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    TArray<FAbilityInfo> AbilityClassArray;

    // 예전 BP에 저장된 기본 공격 클래스. PostLoad에서 어빌리티 배열로 옮긴다.
    UPROPERTY()
    TSubclassOf<UGABaseAttack> BasicAttackAbilityClass_DEPRECATED;

    // 예전 BP 설정을 어빌리티 배열로 한 번 옮겼는지 여부. 이후 기획에서 항목을 지워도 다시 추가하지 않는다.
    UPROPERTY()
    bool bLegacyAbilitiesMigrated = false;


    /** 체력이 감소했지만 살아 있을 때 사용할 피격 몽타주. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Hit Reaction")
    TObjectPtr<UAnimMontage> HitReactionMontage;

    /** Anim BP와 같은 스켈레톤 및 Slot을 사용하는 사망 몽타주. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Death")
    TObjectPtr<UAnimMontage> DeathMontage;

	FDelegateHandle HealthChangedDelegateHandle;

    // 시작 스탯 GE를 이미 적용했는지 여부.
    bool bDefaultStatsApplied = false;

    float MonategeDuration = 0.f;

};
