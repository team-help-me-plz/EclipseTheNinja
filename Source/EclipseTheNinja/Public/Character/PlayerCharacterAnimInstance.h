

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
	void UpdateTurnInPlaceTarget(float TargetYaw);
	void StopTurnInPlace();
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn In Place")
	FName TurnInPlaceSlotName = TEXT("DefaultSlot");

private:
	// 캐릭터 머리 상하 각도 계산
	float CalculateHeadPitch(const APlayerCharacter& Character) const;

	// 캐릭터 머리 업데이트
	void UpdateHeadRotation(const APlayerCharacter& Character);
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveTurnMontage;

	float TurnTargetYaw = 0.0f;
};
