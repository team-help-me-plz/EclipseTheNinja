#pragma once

#include "NativeGameplayTags.h"

// 프로젝트 공용 게임플레이 태그. 여러 클래스에서 함께 쓰는 태그는 이 헤더를 include해서 사용한다.
namespace EclipseGameplayTags
{
	// ---------- 불속성 ----------
	// 불속성 어빌리티 자체를 가리키는 에셋 태그
	ECLIPSETHENINJA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Element_Fire);
	// 불속성이 켜져 있는 동안 플레이어가 가지는 태그
	ECLIPSETHENINJA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_Element_Fire);
	// 화상 중인 대상이 가지는 태그
	ECLIPSETHENINJA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Debuff_Burning);
	// 도트 데미지 GE의 에셋 태그 (피격 경직, 감지 갱신 구분용)
	ECLIPSETHENINJA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Damage_DoT);
	// 불속성 어빌리티 쿨타임
	ECLIPSETHENINJA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Ability_FireElement);

	// 칼 불꽃 연출
	ECLIPSETHENINJA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Fire);
	// 화상 중 적 몸에 붙는 불 연출
	ECLIPSETHENINJA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Enemy_Burn);
	// 화상 틱마다 재생되는 피격 연출
	ECLIPSETHENINJA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Enemy_BurnTick);

	// ---------- SetByCaller ----------
	// 에디터 GE의 SetByCaller는 Data Tag로만 지정할 수 있으므로 값을 넘길 태그를 둔다.
	ECLIPSETHENINJA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Duration);
	ECLIPSETHENINJA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);

	//------------ Stat      ----------
	ECLIPSETHENINJA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invincible);
}
