#include "GAS/Effects/GEBasicAttackDamage.h"
#include "GAS/StatAttributeSet.h"

UGEBasicAttackDamage::UGEBasicAttackDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UStatAttributeSet::GetDamageAttribute();
	Modifier.ModifierOp = EGameplayModOp::AddBase;
	FSetByCallerFloat Magnitude;
	Magnitude.DataName = FName(TEXT("Damage"));
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	Modifiers.Add(Modifier);
}
