#include "GAS/Tags/EclipseGameplayTags.h"

namespace EclipseGameplayTags
{
	// ---------- 불속성 ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Element_Fire, "Ability.Element.Fire", "불속성 어빌리티");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Weapon_Element_Fire, "State.Weapon.Element.Fire", "불속성이 켜져 있는 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Debuff_Burning, "State.Debuff.Burning", "화상 중인 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Damage_DoT, "Effect.Damage.DoT", "도트 데미지 GE");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Ability_FireElement, "Cooldown.Ability.FireElement", "불속성 어빌리티 쿨타임");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_Fire, "GameplayCue.Weapon.Fire", "칼 불꽃 연출");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Enemy_Burn, "GameplayCue.Enemy.Burn", "화상 중 적 몸의 불 연출");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Enemy_BurnTick, "GameplayCue.Enemy.BurnTick", "화상 틱 피격 연출");

	// ---------- SetByCaller ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Duration, "SetByCaller.Duration", "GE 지속시간 SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage, "SetByCaller.Damage", "GE 데미지 SetByCaller");


	//------------ Stat      ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invincible, "State.Invincible", "무적상태 표시 태그");
}
