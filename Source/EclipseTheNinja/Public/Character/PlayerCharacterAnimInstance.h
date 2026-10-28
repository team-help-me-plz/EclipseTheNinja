

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PlayerCharacterAnimInstance.generated.h"

class APlayerCharacter;
class UAnimSequence;
class UAnimMontage;

/** 커서 방향을 AnimGraph에서 사용할 고개 회전으로 변환한다. */
UCLASS()
class ECLIPSETHENINJA_API UPlayerCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeUninitializeAnimation() override;

	bool TryStartTurnInPlace(float TargetYaw);
	// 이미 일부 돈 상태에서 남은 회전(RemainingDeltaYaw, 부호 포함)을 턴 클립으로 마무리한다.
	// 클립과 같은 방향이면 남은 회전량이 같아지는 지점부터 정재생하고, 반대 방향이면 그 각도만큼 역재생한다.
	bool TryStartTurnInPlaceRemaining(float TargetYaw, float RemainingDeltaYaw);
	void UpdateTurnInPlaceTarget(float TargetYaw);
	void StopTurnInPlace();
	// 루트 모션을 끄지 않고 턴을 끝낸다. 회전은 호출한 쪽이 맞춘다.
	// 루트 모션을 끄고 블렌드 아웃하면 턴 클립의 루트 회전이 포즈에 남아 몸이 화면 쪽으로 돌아가 보인다.
	void CancelTurnInPlace(float BlendOutTime);
	bool IsTurningInPlace() const;
	float GetTurnTargetYaw() const { return TurnTargetYaw; }

protected:
	// Modify Bone에서 Add to Existing / Component Space로 사용한다.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cursor Look")
	FRotator HeadLookRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Cursor Look")
	float HeadLookPitch = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Cursor Look")
	bool bEnableHeadTracking = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cursor Look")
	FName HeadBoneName = TEXT("head");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cursor Look", meta = (ClampMin = "0", ClampMax = "89", Units = "deg"))
	float MaxHeadPitchUp = 40.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cursor Look", meta = (ClampMin = "0", ClampMax = "89", Units = "deg"))
	float MaxHeadPitchDown = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cursor Look", meta = (ClampMin = "0"))
	float HeadTrackingSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Turn In Place")
	bool bEnableTurnInPlace = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn In Place")
	TObjectPtr<UAnimSequence> TurnInPlaceAnimation;

	// 제자리 턴은 클립의 루트 모션으로 회전한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.1"))
	float TurnInPlacePlayRate = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0"))
	float TurnInPlaceBlendInTime = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0"))
	float TurnInPlaceBlendOutTime = 0.35f;

	// 하체 전용 슬롯. AnimGraph에서 이 슬롯 출력은 Layered Blend Per Bone으로 하체에만 적용하고,
	// 상체는 Idle(칼 뽑은 자세)을 유지한다. 스켈레톤의 DefaultGroup에 같은 이름의 슬롯이 있어야 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn In Place")
	FName TurnInPlaceSlotName = TEXT("TurnInPlace");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wall Attach")
	bool bWallAttached = false;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wall Attach")
	float ClimbingSpeed = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wall Attach")
	float MaxClimbingSpeed = 200.f;

	// AnimGraph에서 서기/앉기 Blend Space를 고르는 데 쓴다.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Crouch")
	bool bIsCrouching = false;

private:
	// 캐릭터 머리 상하 각도 계산
	float CalculateHeadPitch(const APlayerCharacter& Character) const;

	// 캐릭터 머리 업데이트
	void UpdateHeadRotation(const APlayerCharacter& Character);
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveTurnMontage;

	// 턴 클립의 시간별 누적 루트 회전(Yaw). 남은 회전에 맞는 시작 시점을 찾는 데 쓴다.
	void CacheTurnYawCurve();
	TArray<float> TurnYawCurve;
	float TurnYawCurveStep = 0.0f;
	TWeakObjectPtr<UAnimSequence> CachedTurnAnimation;

	float TurnTargetYaw = 0.0f;
};
