

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ComboEnd.generated.h"

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UAnimNotify_ComboEnd : public UAnimNotify
{
	GENERATED_BODY()
	

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
