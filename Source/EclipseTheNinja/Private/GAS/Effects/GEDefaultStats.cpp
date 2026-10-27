#include "GAS/Effects/GEDefaultStats.h"
#include "GAS/StatAttributeSet.h"

UGEDefaultStats::UGEDefaultStats()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
}

void UGEDefaultStats::SetDefaultStats(
	float MaxHealth,
	float Health,
	float AttackPower
)
{
	AddOverrideModifier(UStatAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AddOverrideModifier(UStatAttributeSet::GetHealthAttribute(), Health);
	AddOverrideModifier(UStatAttributeSet::GetAttackPowerAttribute(), AttackPower);
}

void UGEDefaultStats::AddOverrideModifier(
	const FGameplayAttribute& Attribute,
	float Value
)
{
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = Attribute;
	Modifier.ModifierOp = EGameplayModOp::Override;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));

	Modifiers.Add(Modifier);
}

UGEPlayerDefaultStats::UGEPlayerDefaultStats()
{
	SetDefaultStats(50.0f, 50.0f, 10.0f);
}

UGEMeleeEnemyDefaultStats::UGEMeleeEnemyDefaultStats()
{
	SetDefaultStats(50.0f, 50.0f, 20.0f);
}

UGERangedEnemyDefaultStats::UGERangedEnemyDefaultStats()
{
	SetDefaultStats(30.0f, 30.0f, 10.0f);
}

UGEBossEnemyDefaultStats::UGEBossEnemyDefaultStats()
{
	SetDefaultStats(200.0f, 200.0f, 30.0f);
}
