

#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GACooldownBase.h"
#include "GAElementalFire.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

DECLARE_MULTICAST_DELEGATE(FOnElementalFireStateChanged);

/** 공격력 기준 불속성 데미지 계산 결과 */
USTRUCT(BlueprintType)
struct FElementalFireDamageInfo
{
	GENERATED_BODY()

	// 공격 시 바로 들어가는 데미지
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float	HitDamage = 0.0f;

	// 틱당 들어가는 화상 데미지
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float	TickDamage = 0.0f;

	// 화상 시간(초). 화상 GE의 Duration Magnitude(Scalable Float)에서 읽는다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float	BurnDuration = 0.0f;

	// 화상 틱 수 = 화상 시간 / 화상 GE의 Period (내림)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32	TickCount = 0;

	// 한 번의 공격이 주는 총 데미지 = HitDamage + TickDamage * TickCount
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float	TotalDamage = 0.0f;
};

/**
 * 칼에 불속성을 부여하는 어빌리티.
 * 불속성 GE를 적용한 뒤 GE가 끝날 때까지 활성 상태로 기다리고, GE가 끝나면 쿨타임을 시작한다.
 * 지속시간은 불속성 GE, 쿨타임은 CooldownGameplayEffectClass에 지정한 GE에서 조정한다.
 */
UCLASS()
class ECLIPSETHENINJA_API UGAElementalFire : public UGACooldownBase
{
	GENERATED_BODY()

public:
	UGAElementalFire();

	// 불이 붙었을 때 / 꺼졌을 때 (UI용)
	FOnElementalFireStateChanged OnFireElementStarted;
	FOnElementalFireStateChanged OnFireElementEnded;

	// 불이 붙어 있으면 남은 시간과 전체 지속시간을 돌려준다. 꺼져 있으면 false.
	bool GetFireElementTime(float& OutRemaining, float& OutDuration) const;

	// 공격력 기준으로 불속성 데미지를 계산한다. 공격 시 계산과 에디터 미리보기가 같은 식을 쓴다.
	FElementalFireDamageInfo CalcFireDamage(float AttackPower, float Level = 1.0f) const;

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

	inline TSubclassOf<UGameplayEffect>	GetFireBurnEffectClass() const { return (this->FireBurnEffectClass); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	// 칼에 불을 붙이는 GE (GE_ElementalFire)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ElementalFire")
	TSubclassOf<UGameplayEffect> FireElementEffectClass;

	// 불속성 GE가 끝나면(만료/제거) 쿨타임을 시작하고 어빌리티를 종료한다.
	UFUNCTION()
	void OnFireElementRemoved(const FGameplayEffectRemovalInfo& RemovalInfo);

	/** Burn을 적용할 BP class. GE_ElementalFireBurn을 지정하면 된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Fire")
	TSubclassOf<UGameplayEffect>	FireBurnEffectClass;

	/** 화염 속성 적용 시 공격을 할 때 들어가는 기본 데미지 배율 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Fire")
	float	FireHitDamageRatio = 0.4f;

	/** 화염 속성 적용 시 공격 후 들어갈 틱 데미지 배율 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Fire")
	float	FireDotRatio = 0.2f;

	/** 불이 붙어 있는 동안 칼에 붙여 재생할 이펙트 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ElementalFire|Effect")
	TObjectPtr<UNiagaraSystem>	FireSwordSystem = nullptr;

	/** 이펙트를 붙일 캐릭터 메시의 소켓(본) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ElementalFire|Effect")
	FName	FireSwordSocketName = TEXT("weapon_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ElementalFire|Effect")
	FVector	FireSwordLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ElementalFire|Effect")
	FRotator	FireSwordRotationOffset = FRotator::ZeroRotator;

	///** 화염 속성 적용 시 공격 후 들어갈 총 데미지의 최대 배율 */
	//UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attack|Fire")
	//float	FireDotRatio = 0.2f;

#if WITH_EDITORONLY_DATA
	/** 미리보기 기준 공격력. 플레이어의 기본 공격력을 넣어 두면 된다. */
	UPROPERTY(EditDefaultsOnly, Category = "Attack|Fire|Preview", meta = (ClampMin = "0.0"))
	float	PreviewAttackPower = 10.0f;

	/** 위 배율과 PreviewAttackPower로 계산한 결과. 값을 바꾸면 갱신된다. (화상 GE를 바꿨다면 아무 값이나 한 번 건드리면 갱신) */
	UPROPERTY(VisibleDefaultsOnly, Category = "Attack|Fire|Preview")
	FElementalFireDamageInfo	PreviewDamage;
#endif

private:
	// 이번 발동에서 적용한 불속성 GE
	FActiveGameplayEffectHandle FireElementEffectHandle;

	// 이번 발동에서 칼에 붙인 불꽃 이펙트
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent>	FireSwordEffect = nullptr;
};
