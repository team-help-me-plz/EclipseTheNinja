#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BasicAttackHit.generated.h"

/** Place at the impact frame in the basic attack montage. */
UCLASS(meta = (DisplayName = "Basic Attack Hit"))
class ECLIPSETHENINJA_API UAnimNotify_BasicAttackHit : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
