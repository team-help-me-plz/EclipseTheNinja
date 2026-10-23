#include "Noise/AnimNotify_Noise.h"

FString UAnimNotify_Noise::GetNotifyName_Implementation() const
{
    // 타임라인에서 어떤 소음이 붙었는지 바로 보이게 한다.
    return FString::Printf(TEXT("Noise: %s"), *UEnum::GetDisplayValueAsText(NoiseType).ToString());
}
