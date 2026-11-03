#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_ParryWindow.generated.h"

/**
 * 패링 판정 구간을 지정하는 노티파이 스테이트. 패링 몽타주(또는 그 안의 애님 시퀀스)에 넣는다.
 * 구간이 시작되면 실행 중인 GAParry의 판정을 열고, 끝나면 닫는다.
 * 패링 몽타주에 이 노티파이가 하나도 없으면 GAParry는 몽타주 전체를 판정 구간으로 쓴다.
 */
UCLASS(meta = (DisplayName = "Parry Window"))
class ECLIPSETHENINJA_API UAnimNotifyState_ParryWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual FString GetNotifyName_Implementation() const override;
};
