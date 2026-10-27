#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GACooldownBase.h"
#include "GAThrowKunai.generated.h"

class ACharacter;
class AKunaiProjectile;
class UAnimMontage;

/**
* 플레이어/적 공용 투척 어빌리티.
* 입력과 BT는 이 어빌리티를 지급한 뒤 활성화만 한다.
* 쿨타임 UI가 감시할 수 있도록 UGACooldownBase를 상속한다.
*/
UCLASS()
class ECLIPSETHENINJA_API UGAThrowKunai : public UGACooldownBase
{
    GENERATED_BODY()

public:
    UGAThrowKunai();

    static FGameplayTag GetReleaseEventTag();

    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

protected:
    /**
    * 표창을 던질 방향을 계산하는 함수. GA BP에서 재정의할 수 있다.
    * 우선순위: 이벤트 Target > 적의 전투 대상 > 투척 시작 시 저장한 플레이어 커서 > 정면
    * 대상과 커서의 위치는 모두 몽타주 재생 시작 시점에 저장한 값을 사용한다.
    */
    UFUNCTION(BlueprintNativeEvent, Category = "Kunai")
    FVector GetThrowDirection(ACharacter* Character, const FVector& SpawnLocation) const;

    /**
    * Kunai Release 노티파이 이벤트를 받는 태스크 콜백 함수.
    * 실제로 표창을 생성하여 방향을 계산하고 투척한다.
    */
    UFUNCTION()
    void OnKunaiReleased(FGameplayEventData Payload);

    UFUNCTION()
    void OnMontageCompleted();

    UFUNCTION()
    void OnMontageInterrupted();

protected:
    /** 투척 프레임에 Kunai Release Notify가 배치된 애니메이션 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Animation")
    TObjectPtr<UAnimMontage> ThrowMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Projectile")
    TSubclassOf<AKunaiProjectile> ProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Damage")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Damage", meta = (ClampMin = "0.01"))
    float DamageMultiplier = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Projectile", meta = (ClampMin = "1.0", Units = "cm/s"))
    float ProjectileSpeed = 1500.0f;

    /** 수평선에서 위로 조준할 수 있는 최대 각도. 좌우 어느 방향으로 던져도 동일하게 적용한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Aim", meta = (ClampMin = "0.0", ClampMax = "90.0", UIMin = "0.0", UIMax = "90.0", Units = "deg"))
    float MaxUpwardThrowAngle = 60.0f;

    /** 수평선에서 아래로 조준할 수 있는 최대 각도. 양수로 입력한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Aim", meta = (ClampMin = "0.0", ClampMax = "90.0", UIMin = "0.0", UIMax = "90.0", Units = "deg"))
    float MaxDownwardThrowAngle = 60.0f;

    /** None이면 캐릭터 원점, 지정하면 해당 메시 소켓을 기준으로 Offset을 적용한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Projectile")
    FName ThrowSocketName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Projectile", meta = (Units = "cm"))
    FVector SpawnOffset = FVector(0.0f, 0.0f, 0.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Animation", meta = (ClampMin = "0.01"))
    float PlayRate = 1.0f;

private:
    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> RequestedTarget;

    /** 몽타주 재생 시작 시점의 목표 대상 위치. 이벤트 Target 또는 적의 전투 대상이 있을 때만 유효하다. */
    FVector InitialTargetLocation = FVector::ZeroVector;
    bool bHasInitialTargetLocation = false;

    FVector InitialPlayerCursorLocation = FVector::ZeroVector;
    FVector InitialPlayerForward = FVector::ForwardVector;
    bool bHasInitialPlayerCursor = false;
    bool bReleased = false;

};
