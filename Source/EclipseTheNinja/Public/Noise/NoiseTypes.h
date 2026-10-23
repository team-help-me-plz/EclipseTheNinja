#pragma once

#include "CoreMinimal.h"
#include "NoiseTypes.generated.h"

/** 소음 데이터 테이블의 행을 고르는 키. 반경과 세기는 테이블에서 정한다. */
UENUM(BlueprintType)
enum class ENoiseType : uint8
{
    /** 이동 속도에 따라 크기가 정해지는 발소리 */
    Footstep,
    Land,
    Attack
};
