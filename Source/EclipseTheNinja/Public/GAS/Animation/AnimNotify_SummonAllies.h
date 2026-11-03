#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SummonAllies.generated.h"

/** 지원을 부르는 몽타주에서 동료가 기어오르기 시작할 프레임에 배치한다. */
UCLASS(meta = (DisplayName = "Summon Allies"))
class ECLIPSETHENINJA_API UAnimNotify_SummonAllies : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual FString GetNotifyName_Implementation() const override;
};
