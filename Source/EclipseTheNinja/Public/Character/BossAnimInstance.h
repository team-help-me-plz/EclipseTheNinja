#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BossAnimInstance.generated.h"

/**
 * 보스 ABP의 부모 클래스. AnimGraph가 사용할 이동 상태를 매 프레임 계산한다.
 *
 * Locomotion 스테이트 머신
 * - Idle: 멈춰 있을 때
 * - Walk: 전투 단계가 아닐 때의 이동 (순찰, 경계 단계 수색)
 * - Run: 전투 단계에서의 이동 (추격, 마지막 목격 위치로 이동)
 * 전이 규칙은 bIsMoving / bIsWalking / bIsRunning만 읽으면 된다.
 *
 * 자세 걷기
 * 보스가 달리기 페이즈(RunPhase) 전이면 전투 이동을 달리기 대신 자세 걷기로 보여 준다.
 * AnimGraph는 스테이트 머신 포즈와 자세 걷기 포즈를 StanceWalkAlpha로 섞고,
 * 자세 걷기 안에서는 앞/뒤 걷기를 StanceWalkBackwardAlpha로 섞는다.
 */
UCLASS()
class ECLIPSETHENINJA_API UBossAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	/** 수평 이동 속도(cm/s). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement")
	float GroundSpeed = 0.0f;

	/** GroundSpeed가 MoveThreshold보다 크면 true. Idle 전환에 사용한다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement")
	bool bIsMoving = false;

	/** 전투 단계가 아닌 상태에서 이동 중이면 true. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement")
	bool bIsWalking = false;

	/** 전투 단계에서 이동 중이면 true. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement")
	bool bIsRunning = false;

	/** 스테이트 머신 포즈(0)와 자세 걷기 포즈(1)의 혼합 비율. StanceWalkBlendTime 동안 부드럽게 바뀐다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement")
	float StanceWalkAlpha = 0.0f;

	/** 자세 걷기 안에서 앞 걷기(0)와 뒤 걷기(1)의 혼합 비율. 바라보는 반대 방향으로 움직이면 1로 간다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement")
	float StanceWalkBackwardAlpha = 0.0f;

	/** Walk 상태 걷기 애니메이션의 재생 속도. 속도가 달라도 발이 미끄러지지 않도록 GroundSpeed에 비례한다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement")
	float WalkPlayRate = 1.0f;

	/** Run 상태 달리기 애니메이션의 재생 속도. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement")
	float RunPlayRate = 1.0f;

	/** 자세 걷기 애니메이션의 재생 속도. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Movement")
	float StanceWalkPlayRate = 1.0f;

	/** 이동 중으로 판단할 최소 속도(cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MoveThreshold = 10.0f;

	/** 걷기 애니메이션을 1배속으로 재생했을 때 맞는 이동 속도(cm/s). 발이 미끄러져 보이면 이 값을 조정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "1.0", Units = "cm/s"))
	float WalkAnimSpeed = 150.0f;

	/** 달리기 애니메이션을 1배속으로 재생했을 때 맞는 이동 속도(cm/s). 발이 미끄러져 보이면 이 값을 조정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "1.0", Units = "cm/s"))
	float RunAnimSpeed = 300.0f;

	/**
	 * 자세 걷기 애니메이션을 1배속으로 재생했을 때 맞는 이동 속도(cm/s). 발이 미끄러져 보이면 이 값을 조정한다.
	 * Anim_Stance_WalkFwd의 접지 발 속도 74.7cm/s(BossRunLoop 커맨드릿 -Analyze로 측정)에 보스 스케일 1.25를 곱한 값.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "1.0", Units = "cm/s"))
	float StanceWalkAnimSpeed = 93.0f;

	/** 자세 걷기로 들어가고 나오거나 앞뒤 걷기를 바꿀 때 섞는 시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", Units = "s"))
	float StanceWalkBlendTime = 0.2f;

private:
	// 이동 속도를 애니메이션 기준 속도로 나눈 재생 속도. 너무 느리거나 빠르지 않게 제한한다.
	float CalculatePlayRate(float AnimSpeed) const;

	// 혼합 비율을 StanceWalkBlendTime에 맞춰 목표값으로 옮긴다.
	float BlendAlphaTo(float Current, float Target, float DeltaSeconds) const;
};
