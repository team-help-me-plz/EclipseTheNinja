#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_MeshTremble.generated.h"

/**
 * 노티파이 구간 동안 메시를 잘게 떨게 하는 노티파이 스테이트. 기를 모으다 터지기 직전의 "부르르" 떨림에 쓴다.
 * 메시의 상대 위치만 흔들고 캡슐(액터)은 움직이지 않는다. 구간이 끝나면 원래 위치로 되돌린다.
 * 애니메이션 에디터 미리보기에서도 보인다.
 */
UCLASS(meta = (DisplayName = "Mesh Tremble"))
class ECLIPSETHENINJA_API UAnimNotifyState_MeshTremble : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_MeshTremble();

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual void NotifyTick(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 떨림의 최대 이동 거리(cm). 메시 기준 X(앞뒤), Y(좌우), Z(위아래)마다 따로 정한다. */
	UPROPERTY(EditAnywhere, Category = "Tremble", meta = (Units = "cm"))
	FVector Amplitude = FVector(6.0f, 6.0f, 3.0f);

	/** 1초에 떨림 방향을 바꾸는 횟수. 높을수록 잘게 떨린다. */
	UPROPERTY(EditAnywhere, Category = "Tremble", meta = (ClampMin = "1.0", Units = "Hz"))
	float Frequency = 40.0f;

	/** 구간 끝으로 갈수록 떨림을 키운다. 꺼 두면 처음부터 끝까지 같은 세기로 떤다. */
	UPROPERTY(EditAnywhere, Category = "Tremble")
	bool bRampUp = true;

private:
	// 노티파이 객체는 몽타주 하나를 여러 메시가 함께 쓰므로 메시마다 상태를 따로 둔다.
	struct FTrembleState
	{
		FVector BaseLocation = FVector::ZeroVector;
		float Duration = 0.0f;
		float Elapsed = 0.0f;
		float SinceChange = 0.0f;
	};

	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FTrembleState> States;
};
