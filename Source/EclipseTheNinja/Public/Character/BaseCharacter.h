#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "BaseCharacter.generated.h"

class UAbilitySystemComponent;
class UStatAttributeSet;
class UAnimMontage;
class UGABaseAttack;

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
    void DoBasicAttack();

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
    void InitializeAbilitySystem();
    void GiveBasicAttackAbility();

private:
    UAnimMontage* GetBasicAttackMontage(const ACharacter* Character) const;


protected:
    // 기존 BP 설정을 유지하도록 컴포넌트와 프로퍼티 이름을 보존한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<UAbilitySystemComponent> ASC;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<UStatAttributeSet> StatAttributeSet;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UAnimMontage> BasicAttackMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    TSubclassOf<UGABaseAttack> BasicAttackAbilityClass;


    /** 체력이 감소했지만 살아 있을 때 사용할 피격 몽타주. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Hit Reaction")
    TObjectPtr<UAnimMontage> HitReactionMontage;

    /** Anim BP와 같은 스켈레톤 및 Slot을 사용하는 사망 몽타주. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Death")
    TObjectPtr<UAnimMontage> DeathMontage;

};
