
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "StatAttributeSet.generated.h"

// 메타 Damage를 Health로 변환한 뒤 원본 피해 Context를 전달한다.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnStatDamageReceived, const FGameplayEffectContextHandle&);

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UStatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	UStatAttributeSet();

	FOnStatDamageReceived OnDamageReceived;
	FOnStatDamageReceived OnDotDamageReceived;

	/** 무적이라 피해를 받지 않았을 때 원본 피해 Context를 전달한다. 체력은 그대로지만 공격받은 것을 알아채야 하는 쪽(적 감지 등)에 쓴다. */
	FOnStatDamageReceived OnDamageBlocked;

	// CurrentValue 변경 전에 실행되는 함수
	// 값의 Clamping용도로 사용
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/**
	 * BaseValue 변경 전에 실행되는 함수
	 * 회복 GE처럼 Health에 직접 더하는 Instant GE가 BaseValue를 MaxHealth 위로 올리지 않도록 Clamp한다.
	 */
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	// 이펙트가 적용 된 후에 실행되는 함수
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;


	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UStatAttributeSet, Health);

	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UStatAttributeSet, MaxHealth);

	UPROPERTY(BlueprintReadOnly, Category = "Attack Stat")
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS_BASIC(UStatAttributeSet, AttackPower);

	UPROPERTY(BlueprintReadOnly, Category = "Meta Attribute")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS_BASIC(UStatAttributeSet, Damage);

	UPROPERTY(BlueprintReadOnly, Category = "Meta Attribute")
	FGameplayAttributeData DotDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UStatAttributeSet, DotDamage);
};
