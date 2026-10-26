#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NoiseTypes.generated.h"

inline constexpr ECollisionChannel ECC_Noise = ECC_GameTraceChannel3;

/** 소음 데이터 테이블의 행을 고르는 키. 반경과 세기는 테이블에서 정한다. */
UENUM(BlueprintType)
enum class ENoiseType : uint8
{
    /** 이동 속도에 따라 크기가 정해지는 발소리 */
    Footstep,
    Jump,
    Land,
    Attack,
	Kunai,
	Wallstep,
	WallJump,
};

/** 소음 데이터 테이블(DT_Noise)의 행. 행 이름은 ENoiseType의 이름(Footstep, Land...)과 같아야 한다. */
USTRUCT(BlueprintType)
struct ECLIPSETHENINJA_API FNoiseTableRow : public FTableRowBase
{
    GENERATED_BODY()

    /** 이 거리 안의 적만 소리를 듣는다. 화면의 소음 링 크기와 같다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Noise", meta=(ClampMin="0.0", Units="cm"))
    float Radius = 500.0f;

    /** 소리가 난 지점에서 들었을 때의 주목도 증가량. 거리에 따라 반경 끝에서 0까지 줄어든다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Noise", meta=(ClampMin="0.0", UIMax="1.0"))
    float Strength = 1.0f;

    static FName GetRowName(ENoiseType Type)
    {
        return FName(StaticEnum<ENoiseType>()->GetNameStringByValue(static_cast<int64>(Type)));
    }
};
