#pragma once

#include "CoreMinimal.h"
#include "AbilityInfo.generated.h"

class UGameplayAbility;

/** 어빌리티를 부여·발동할 때 쓰는 GAS 입력 ID. 기존 BP의 클론술 값(100)을 유지한다. */
UENUM(BlueprintType)
enum class EAbilityInputID : uint8
{
    None = 0,
    BasicAttack = 1,
    Assassinate = 2,
    AerialAssassinate = 3,
    BelowAssassinate = 4,
    ThrowKunai = 5,
    WallAttach = 6,
    LedgeHang = 7,
    LedgeClimb = 8,
    WallSlide = 9,
    CloneTech = 100,
	ElementalFire = 200,
};

/** GAS 입력 ID로 변환한다. None은 어떤 입력과도 연결하지 않는다. */
inline int32 ToAbilityInputID(EAbilityInputID InputID)
{
    return InputID == EAbilityInputID::None ? INDEX_NONE : static_cast<int32>(InputID);
}

/** Designer-assigned startup ability and its GAS input identifier. */
USTRUCT(BlueprintType)
struct FAbilityInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<UGameplayAbility> AbilityClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
    int32 AbilityLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EAbilityInputID InputID = EAbilityInputID::None;
};
