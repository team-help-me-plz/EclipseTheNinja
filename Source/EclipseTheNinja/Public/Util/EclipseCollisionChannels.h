#pragma once

#include "CoreMinimal.h"

/**
 * 프로젝트 공용 Collision Channel
 * 번호는 Config/DefaultEngine.ini의 DefaultChannelResponses와 반드시 같아야 한다.
 */


inline constexpr ECollisionChannel ECC_Vine = ECC_GameTraceChannel5;

// 몬스터 캡슐의 오브젝트 타입. 기본 반응은 Block이고, 플레이어와 몬스터 캡슐만 Ignore로 둬서 서로 통과한다.
inline constexpr ECollisionChannel ECC_Monster = ECC_GameTraceChannel7;
