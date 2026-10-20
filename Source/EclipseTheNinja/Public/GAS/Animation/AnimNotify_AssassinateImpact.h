#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_AssassinateImpact.generated.h"

/** 암살자 몽타주에서 칼이 몸에 박히는 프레임에 둔다. 이 순간에 히트스톱·슬로모션이 걸린다. */
UCLASS(meta = (DisplayName = "Assassinate Impact"))
class ECLIPSETHENINJA_API UAnimNotify_AssassinateImpact : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
