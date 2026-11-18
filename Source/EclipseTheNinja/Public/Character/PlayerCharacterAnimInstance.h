

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PlayerCharacterAnimInstance.generated.h"

class APlayerCharacter;
class UAnimSequence;
class UAnimMontage;
struct FHitResult;

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
	// 발 IK는 AnimGraph 노드 없이 프록시에서 그래프 결과 포즈에 바로 적용한다.
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	/** 땅 위에 있을 때 발을 실제 바닥(지붕 경사·용마루 등) 높이와 기울기에 맞춘다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Foot IK")
	bool bEnableFootIK = true;

	/** 메시 바닥 기준으로 발 위쪽 몇 uu부터 아래로 트레이스할지 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK", meta = (ClampMin = "0"))
	float FootTraceUp = 50.0f;

	/** 메시 바닥 기준으로 아래로 몇 uu까지 바닥을 찾을지 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK", meta = (ClampMin = "0"))
	float FootTraceDown = 60.0f;

	/** 발을 올릴 수 있는 최대 높이 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK", meta = (ClampMin = "0"))
	float MaxFootRaise = 40.0f;

	/** 발을 내릴 수 있는 최대 깊이 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK", meta = (ClampMin = "0"))
	float MaxFootDrop = 40.0f;

	/** 골반을 내릴 수 있는 최대 깊이. 용마루 뒤 경사로 빠진 발 때문에 쪼그려 앉아 보이지 않게 작게 둔다.
	 *  다리가 닿지 않는 만큼은 다리를 편 채로 둔다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK", meta = (ClampMin = "0"))
	float MaxPelvisDrop = 10.0f;

	/** 발을 바닥 기울기에 맞춰 돌리는 최대 각도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK", meta = (ClampMin = "0", ClampMax = "80", Units = "deg"))
	float MaxFootSlopeAngle = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK", meta = (ClampMin = "0"))
	float FootIKInterpSpeed = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	FName PelvisBoneName = TEXT("pelvis");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	FName LeftThighBoneName = TEXT("thigh_l");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	FName LeftCalfBoneName = TEXT("calf_l");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	FName LeftFootBoneName = TEXT("foot_l");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	FName RightThighBoneName = TEXT("thigh_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	FName RightCalfBoneName = TEXT("calf_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	FName RightFootBoneName = TEXT("foot_r");

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

	// 앉은 상태의 제자리 턴. 비어 있으면 서서 하는 턴 클립을 쓴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn In Place")
	TObjectPtr<UAnimSequence> CrouchTurnInPlaceAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.1"))
	float CrouchTurnInPlacePlayRate = 1.0f;

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
	/** 발 아래 바닥을 찾아 발·골반 높이 보정값을 갱신하고 프록시에 넘긴다 */
	void UpdateFootIK(const APlayerCharacter* Character, float DeltaSeconds);

	/** 발 아래 보이는 바닥을 찾는다. 숨긴 보조 박스와 폰은 건너뛴다 */
	bool TraceFootGround(const APlayerCharacter& Character, const FVector& Start, const FVector& End, FHitResult& OutHit) const;

	// 월드 Z 보정값(uu)과 바닥 법선. 보간된 값이다.
	float LeftFootOffset = 0.0f;
	float RightFootOffset = 0.0f;
	float PelvisOffset = 0.0f;
	FVector LeftFootNormal = FVector::UpVector;
	FVector RightFootNormal = FVector::UpVector;

	// 캐릭터 머리 상하 각도 계산
	float CalculateHeadPitch(const APlayerCharacter& Character) const;

	// 캐릭터 머리 업데이트
	void UpdateHeadRotation(const APlayerCharacter& Character);
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveTurnMontage;

	// 지금 턴 몽타주가 재생 중인 클립(서기/앉기). 턴 도중 자세가 바뀌었는지 비교한다.
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ActiveTurnAnimation;

	// 턴 도중 앉기 상태가 바뀌면 남은 회전을 새 자세의 턴 클립으로 이어서 재생한다.
	void SwapTurnClipForCrouch();

	// 남은 회전(부호 포함)에 맞는 지점부터 턴 클립을 재생한다. 재생 중인 몽타주 여부는 호출한 쪽이 확인한다.
	bool PlayTurnClipRemaining(UAnimSequence* TurnAnimation, float TargetYaw, float RemainingDeltaYaw);

	// 앉기 여부에 맞는 턴 클립과 재생 속도
	UAnimSequence* GetTurnInPlaceAnimation() const;
	float GetTurnInPlacePlayRate() const;

	// 턴 클립의 시간별 누적 루트 회전(Yaw). 남은 회전에 맞는 시작 시점을 찾는 데 쓴다.
	void CacheTurnYawCurve(UAnimSequence* TurnAnimation);
	TArray<float> TurnYawCurve;
	float TurnYawCurveStep = 0.0f;
	// 캐시된 누적 곡선에서 Time(초) 시점의 누적 Yaw를 보간해 구한다.
	float GetTurnYawAtTime(float Time) const;
	TWeakObjectPtr<UAnimSequence> CachedTurnAnimation;

	float TurnTargetYaw = 0.0f;
};
