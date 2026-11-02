#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_AssassinateVFX.generated.h"

/** 암살자 몽타주의 원하는 프레임에 배치한다. 암살 GA의 VFX 설정으로 피격자에게 이펙트를 재생한다. */
UCLASS(meta = (DisplayName = "Assassinate VFX"))
class ECLIPSETHENINJA_API UAnimNotify_AssassinateVFX : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
