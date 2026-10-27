


#include "GAS/Effects/GEElementalFire.h"
#include "GAS/Tags/EclipseGameplayTags.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGEElementalFire::UGEElementalFire()
{
	// 칼에 불이 붙어 있는 시간. BP(GE_ElementalFire)의 Duration Magnitude에서 조정한다
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(10.0f));

	// 대상(플레이어)에게 Tag 부여
	UTargetTagsGameplayEffectComponent*	TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer	GrantedTags;
	GrantedTags.AddTag(EclipseGameplayTags::State_Weapon_Element_Fire);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(TargetTags);

	// 칼 불꽃 Cue. Cue BP가 없으면 아무 일도 안 일어난다
	GameplayCues.Add(FGameplayEffectCue(EclipseGameplayTags::GameplayCue_Weapon_Fire, 0.f, 1.f));
}
