#pragma once

#include "CoreMinimal.h"
#include "PlayerCondition.generated.h"

/** 적과의 관계로 정해지는 플레이어의 상황. 우선순위는 Combat > Hidden > Normal이다. */
UENUM(BlueprintType)
enum class EPlayerCondition : uint8
{
    /** 평상시. 적에게 감지될 수 있다. */
    Normal,
    /** 은신 지점에 숨어 있다. 적에게 감지되지 않는다. */
    Hidden,
    /** 적에게 발각되어 전투 중이다. 은신과 분신 회수를 할 수 없다. */
    Combat,
};
