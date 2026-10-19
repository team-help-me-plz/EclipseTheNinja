#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_KunaiRelease.generated.h"

/** 투척 몽타주에서 쿠나이가 손을 떠나는 프레임에 배치한다. */
UCLASS(meta = (DisplayName = "Kunai Release"))
class ECLIPSETHENINJA_API UAnimNotify_KunaiRelease : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(
        USkeletalMeshComponent* MeshComp,
        UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
    virtual FString GetNotifyName_Implementation() const override;

};
