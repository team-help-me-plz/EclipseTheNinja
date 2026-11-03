#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_SlowMotion.generated.h"

/**
 * 노티파이 구간 동안 게임 전체 배속을 낮추는 노티파이 스테이트. 패링 성공 같은 연출 몽타주에 넣는다.
 * 구간이 끝나거나 몽타주가 끊기면 1배속으로 되돌린다.
 * 배속이 낮아지면 몽타주도 같이 느려지므로, 실제로 느려지는 시간은 구간 길이 ÷ 배속이 된다.
 * 애니메이션 에디터 미리보기에서는 배속을 바꾸지 않는다.
 */
UCLASS(meta = (DisplayName = "Slow Motion"))
class ECLIPSETHENINJA_API UAnimNotifyState_SlowMotion : public UAnimNotifyState
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

protected:
	/** 구간 동안의 게임 배속. 1이면 느려지지 않는다. */
	UPROPERTY(EditAnywhere, Category = "SlowMotion", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float TimeDilation = 0.2f;
};
