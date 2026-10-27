

#include "GAS/StatAttributeSet.h"

UStatAttributeSet::UStatAttributeSet()
{
    InitHealth(100.0f);
    InitMaxHealth(100.0f);
    InitAttackPower(10.0f);
    InitDamage(0.0f);
    InitDotDamage(0.0f);
}

void UStatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetHealthAttribute())
    {
        // Health가 변경되려고 해서 호출되었다.
        NewValue = FMath::Clamp(NewValue, 0, GetMaxHealth());
    }
    else if (Attribute == GetMaxHealthAttribute())
    {
        // MaxHealth가 변경되려고 해서 호출되었다.
        NewValue = FMath::Max(0, NewValue);
    }
}

void UStatAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
    Super::PreAttributeBaseChange(Attribute, NewValue);

    if (Attribute == GetHealthAttribute())
    {
        // Health의 BaseValue가 MaxHealth를 넘지 않도록 한다. (회복 GE가 Health에 직접 더하는 경우)
        NewValue = FMath::Clamp(NewValue, 0, GetMaxHealth());
    }
}

void UStatAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    // BaseValue가 변경될 때만 실행된다.
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetDamageAttribute())
    {
        // 이팩트로 인해 변경된 어트리뷰트가 Damage다
        const float LocalDamage = GetDamage();
        SetDamage(0.0f);	// [가장 중요] : 메타어트리뷰트는 사용했으면 비워야 한다.

        if (LocalDamage > 0)
        {
            //방어력 등 고려 사항이 있으면 여기서 계산
            float FinalDamage = LocalDamage;	// 각종 계산 추가(방어력, 최소대미지보장, 쉴드, 피해 증가 등등)
            FinalDamage = FMath::Max(1.0f, FinalDamage);			// 최소대미지 보장

            const float OldHealth = GetHealth();
            const float NewHealth = FMath::Clamp(OldHealth - FinalDamage, 0.0f, GetMaxHealth());
            SetHealth(NewHealth);

            // SetHealth()의 체력 변경 델리게이트에는 이 GameEffect의 Context가 전달되지 않는다.
            // 따라서 원본 Context가 유효한 이 콜백에서 브로드캐스트를 통해 피해 출처를 별도로 알린다.
            if (NewHealth < OldHealth)
            {
                OnDamageReceived.Broadcast(Data.EffectSpec.GetContext());
            }
        }
    }
    else if (Data.EvaluatedData.Attribute == GetDotDamageAttribute())
    {
        // 이팩트로 인해 변경된 어트리뷰트가 DotDamage다
        const float LocalDotDamage = GetDotDamage();
        SetDotDamage(0.0f);	// [가장 중요] : 메타어트리뷰트는 사용했으면 비워야 한다.

        if (LocalDotDamage > 0)
        {
            //방어력 등 고려 사항이 있으면 여기서 계산
            float FinalDotDamage = LocalDotDamage;	// 각종 계산 추가(방어력, 최소대미지보장, 쉴드, 피해 증가 등등)
			FinalDotDamage = FMath::Max(1.0f, FinalDotDamage);			// 최소대미지 보장

            const float OldHealth = GetHealth();
            const float NewHealth = FMath::Clamp(OldHealth - FinalDotDamage, 0.0f, GetMaxHealth());

            // SetHealth()의 체력 변경 델리게이트에는 이 GameEffect의 Context가 전달되지 않는다.
            // 따라서 원본 Context가 유효한 이 콜백에서 브로드캐스트를 통해 피해 출처를 별도로 알린다.
            if (NewHealth < OldHealth)
            {
                OnDotDamageReceived.Broadcast(Data.EffectSpec.GetContext());
            }
            SetHealth(NewHealth);
        }
    }
}


