#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Noise/NoiseTypes.h"
#include "AnimNotify_Noise.generated.h"

/** 게임 판정용 소음이 나는 프레임에 배치한다. 실제 SFX와는 별개다. */
UCLASS(meta = (DisplayName = "Noise"))
class ECLIPSETHENINJA_API UAnimNotify_Noise : public UAnimNotify
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise")
    ENoiseType NoiseType = ENoiseType::Footstep;

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

    virtual FString GetNotifyName_Implementation() const override;
};
