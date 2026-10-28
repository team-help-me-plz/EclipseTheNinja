#include "GAS/Damage/FireDamageType.h"

UFireDamageType::UFireDamageType()
{
	// 물리 시뮬레이션 중인 오브젝트(밀 수 있는 상자 등)가 불속성 타격에 밀려나지 않게 한다.
	DamageImpulse = 0.0f;
	DestructibleImpulse = 0.0f;
}
