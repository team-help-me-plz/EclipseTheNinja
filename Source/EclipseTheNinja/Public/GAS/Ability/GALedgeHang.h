

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GALedgeHang.generated.h"

class UAnimMontage;
class UCurveVector;

/**
 * 서 있는 상태에서 발판 모서리 아래 벽으로 내려가 매달린다.
 * 몽타주가 있으면 몽타주 루트 모션이 진행한 비율을 따라 매달림 위치까지 코드가 이동시킨다(하단 암살의 올라가기와 같은 방식).
 * 몽타주가 없으면 모서리 밖으로 한 걸음 나간 뒤 아래로 내려가는 두 구간으로 이동한다.
 * 매달림 위치에 도착하면 내려가기 끝 자세를 정지 몽타주로 붙잡아 두고, 이 어빌리티를 끝낸 뒤 GAWallAttach에 매달린 상태를 넘긴다.
 */
UCLASS()
class ECLIPSETHENINJA_API UGALedgeHang : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGALedgeHang();

	// 모서리에서 내려가거나 올라가는 동안 붙는 태그. 이동·커서 회전을 막는다.
	static FGameplayTag GetLedgeHangingTag();

	// 모서리에서 내려와 매달릴 때 GAWallAttach에 넘기는 표시. 매달리기는 이 태그가 있으면 메시 오프셋 대신 정지 몽타주 자세를 쓴다.
	static FGameplayTag GetLedgeHoldTag();

	// 손 맞추기로 내려가는 동안 메시 위치를 매달리기 Idle에 맞춰 둔 경우 GAWallAttach에 넘기는 표시.
	// 매달리기는 이 태그가 있으면 메시를 그대로 두고, 끝날 때만 기본 위치로 되돌린다.
	static FGameplayTag GetLedgeMeshAlignedTag();

	// 내려가는 도중 애님 BP가 매달리기 상태로 미리 넘어가 있도록 붙이는 태그.
	// 몽타주가 덮고 있는 동안 상태 머신 전환이 끝나, 몽타주가 빠질 때 바로 매달리기 Idle로 섞인다.
	static FGameplayTag GetLedgeHangPoseTag();
	/**
	 * Montage의 [0, 첫 구간 끝] 루트 모션이 진행한 비율을 따라 Start에서 End까지 가는 경로 곡선을 Curve에 만든다.
	 * 실제 거리·높이가 애니메이션과 달라도 같은 가감속 궤적이 된다(GAAssassinateBase::MoveAlongAttackRootMotion과 같은 방식).
	 * 루트 모션이 수평·수직으로 거의 없으면 false.
	 */
	static bool BuildRootMotionPath(const UAnimMontage* Montage, const FVector& Start, const FVector& End,
		UCurveVector* Curve, float& OutDuration);

	// 몽타주 시간 [StartTime, EndTime] 구간으로 같은 경로 곡선을 만든다. 수평 이동만 있는 구간도 허용한다.
	static bool BuildRootMotionPathInRange(const UAnimMontage* Montage, float StartTime, float EndTime,
		const FVector& Start, const FVector& End, UCurveVector* Curve);

	// 몽타주 SegmentIndex번째 구간의 루트 모션 이동량. INDEX_NONE이면 마지막 구간. 몽타주가 없으면 0.
	static FVector GetMontageRootMotion(const UAnimMontage* Montage, int32 SegmentIndex = 0);

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	struct FLedgePlan
	{
		// 모서리 밖으로 나간 위치 (선 높이 그대로)
		FVector StepOutLocation = FVector::ZeroVector;
		// 두 구간 몽타주에서 첫 구간(앉기)을 하는 자리. S를 누른 자리에서 그대로 앉는다.
		FVector EdgeStandLocation = FVector::ZeroVector;
		// 매달린 위치 (캡슐 중심)
		FVector HangLocation = FVector::ZeroVector;
		// 내려가기가 끝나는 위치. 매달림 위치보다 MeshZOffset만큼 위 (메시 기본 위치 기준으로 매달리기 Idle과 같은 높이)
		FVector DropEndLocation = FVector::ZeroVector;
		// 매달리기(GAWallAttach)가 메시에 더하는 높이 오프셋
		float MeshZOffset = 0.f;
		// 벽에서 바깥쪽 방향 (X축 +1 / -1). 매달리면 반대 방향을 바라본다.
		float OutDir = 1.f;
		// 손 맞추기: 내려가는 동안 메시를 기본 위치에서 이만큼(메시 상대 좌표) 옮겨 내려가기 끝 자세의 손이 모서리에 닿게 한다
		FVector MeshShift = FVector::ZeroVector;
		// 매달리기 Idle일 때의 메시 이동. 내려가기 몽타주가 섞여 빠지는 동안 MeshShift에서 이 값으로 옮겨 간다.
		FVector IdleMeshShift = FVector::ZeroVector;
		bool bHandAligned = false;
		// 디버그용: 모서리 꼭짓점, 손이 닿아야 할 지점, 매달렸을 때 예상 손 위치(메시 이동 포함)
		FVector LedgeCorner = FVector::ZeroVector;
		FVector HandTarget = FVector::ZeroVector;
		FVector PredictedHand = FVector::ZeroVector;
	};

	// 바라보는 방향을 먼저, 그 다음 등 뒤 방향의 모서리를 찾는다.
	bool FindLedge(const ACharacter* Player, FLedgePlan& OutPlan) const;
	bool FindLedgeInDirection(const ACharacter* Player, float OutDir, FLedgePlan& OutPlan) const;

	// 모서리 윗면에서 매달린 캡슐 중심까지의 깊이.
	// 내려가기 몽타주의 루트 모션이 내려간 높이를 쓰면 몽타주 끝 자세의 손이 모서리에 맞는다.
	float ResolveHangDepth(float CapsuleHalfHeight) const;

	// 매달리기(GAWallAttach)가 메시에 더할 높이 오프셋. 정지 자세를 쓰거나 매달리기가 없으면 0.
	float GetHangMeshZOffset(const ACharacter* Player) const;

	// 내려가기 몽타주 끝 자세에서 손 본(HandBoneNames 평균)의 위치를, 매달려 벽을 바라보는 캡슐 중심 기준 월드 오프셋으로 구한다.
	// 메시는 기본 상대 위치로 본다. 몽타주나 본이 없으면 false.
	bool GetDropEndHandOffset(const ACharacter* Player, float OutDir, FVector& OutOffset) const;

	// 내려가는 동안 메시를 Plan.MeshShift까지 Duration 동안 옮기고, 내려가기 몽타주가 섞여 빠지는 만큼 Plan.IdleMeshShift로 옮긴다.
	// 어빌리티가 끝나 매달리기로 넘어간 뒤에도 몽타주가 다 빠질 때까지 이어진다 (매달리기가 끝나면 멈춘다).
	void StartMeshShiftBlend(ACharacter* Player, float Duration);
	void ScheduleMeshShiftTick(int32 Serial);
	// 계속해야 하면 true
	bool TickMeshShiftBlend();
	// 내려가기 몽타주가 화면에 섞여 있는 정도. 재생 중이면 1, 섞여 빠지는 중이면 그 가중치, 없으면 0.
	float GetDropMontageWeight(const ACharacter* Player) const;
	void DrawDropDebug(const ACharacter* Player) const;

	UFUNCTION()
	void OnStepOutFinished();

	// 두 구간 몽타주에서, 두 번째 구간이 시작하기 전에 그 구간부터 다시 재생해 두 자세를 섞는다 (하단 암살과 같은 방식)
	UFUNCTION()
	void OnSectionCrossfade();

	// 내려가기 몽타주가 다 섞여 들어온 뒤 매달리기 자세 태그를 붙인다
	UFUNCTION()
	void OnHangPoseReady();

	UFUNCTION()
	void OnDropFinished();

	UFUNCTION()
	void OnInterrupted();

	// 모서리에서 이 거리 안에 서 있어야 내려갈 수 있다 (캡슐 표면 기준)
	UPROPERTY(EditDefaultsOnly, Category = "LedgeHang", meta = (ClampMin = "0.0", Units = "cm"))
	float EdgeSearchDistance = 25.f;

	// 켜면 매달림 깊이를 내려가기 몽타주의 루트 모션 높이에서 구한다. 몽타주에 루트 모션이 없으면 HangDepth를 쓴다.
	UPROPERTY(EditDefaultsOnly, Category = "LedgeHang")
	bool bHangDepthFromMontage = true;

	// 모서리 윗면에서 매달린 캡슐 중심까지의 깊이. 매달리기 애니메이션의 손 높이에 맞춘다.
	// 매달리기 판정(머리 높이 트레이스)을 통과하려면 캡슐 반높이의 절반보다 충분히 커야 한다.
	UPROPERTY(EditDefaultsOnly, Category = "LedgeHang", meta = (ClampMin = "0.0", Units = "cm"))
	float HangDepth = 110.f;

	// 매달린 발밑과 바닥 사이에 필요한 최소 높이. GAWallAttach의 MinHeightAboveGround 이상이어야 한다.
	UPROPERTY(EditDefaultsOnly, Category = "LedgeHang", meta = (ClampMin = "0.0", Units = "cm"))
	float MinHeightAboveGround = 40.f;

	// 몽타주가 없을 때 모서리 밖으로 나가는 시간
	UPROPERTY(EditDefaultsOnly, Category = "LedgeHang", meta = (ClampMin = "0.05", Units = "s"))
	float StepOutTime = 0.15f;

	// 몽타주가 없을 때 매달림 위치까지 내려가는 시간
	UPROPERTY(EditDefaultsOnly, Category = "LedgeHang", meta = (ClampMin = "0.05", Units = "s"))
	float DropTime = 0.2f;

	// 내려가며 재생할 몽타주 (선택). 루트 모션이 있으면 그 궤적을 따라 이동하고, 몽타주 자체의 루트 모션은 끈다.
	// 구간이 둘이면 [제자리에서 앉기 + 아래로 내려가기]로 보고, 첫 구간 동안 제자리에 있다가 둘째 구간 동안 매달림 위치로 이동한다.
	// 두 구간 경계는 몽타주 Blend In 시간만큼 먼저 둘째 구간을 다시 재생해 섞는다.
	// 매달린 뒤에는 HoldMontage가 있으면 그 자세를, 없으면 애님 BP의 매달리기 상태가 이어받는다.
	UPROPERTY(EditDefaultsOnly, Category = "LedgeHang")
	TSoftObjectPtr<UAnimMontage> DropMontage;

	// 매달린 동안 붙잡아 둘 자세 몽타주 (선택). 첫 프레임이 내려가기 몽타주의 끝 자세와 같아야 한다.
	// 재생 직후 멈춰 두고, 매달리기가 끝날 때 GAWallAttach가 멈춘다. 비우면 애님 BP의 매달리기 상태가 이어받는다.
	UPROPERTY(EditDefaultsOnly, Category = "LedgeHang")
	TSoftObjectPtr<UAnimMontage> HoldMontage;

	// 매달리기 Idle로 넘어갈 때 내려가기 몽타주가 섞여 빠지는 시간. 길수록 부드럽게 넘어간다.
	UPROPERTY(EditDefaultsOnly, Category = "LedgeHang", meta = (ClampMin = "0.0", Units = "s"))
	float HangBlendOutTime = 0.4f;

	// 켜면 내려가기 몽타주 끝 자세의 손 본 위치로 매달림 높이를 정하고, 벽 쪽 차이는 내려가는 동안 메시를 옮겨 맞춘다.
	// 손 본을 찾지 못하면 기존 방식(bHangDepthFromMontage / HangDepth)을 쓴다.
	UPROPERTY(EditDefaultsOnly, Category = "LedgeHang|Hand")
	bool bAlignHandToLedge = true;

	// 모서리에 맞출 손 본. 여러 개면 평균 위치를 쓴다.
	UPROPERTY(EditDefaultsOnly, Category = "LedgeHang|Hand", meta = (EditCondition = "bAlignHandToLedge"))
	TArray<FName> HandBoneNames = { TEXT("hand_l"), TEXT("hand_r") };

	// 손을 모서리 꼭짓점에서 옮길 거리. X: 윗면 안쪽(+)으로, Y: 위(+)로. 손 본 중심이 손바닥보다 위에 있으면 Y를 올린다.
	UPROPERTY(EditDefaultsOnly, Category = "LedgeHang|Hand", meta = (EditCondition = "bAlignHandToLedge", Units = "cm"))
	FVector2D HandLedgeAdjust = FVector2D(3.f, 3.f);

	// 매달리기 Idle 자세에서 손 본이 캡슐 중심보다 얼마나 위에 있는지 (매달리기의 메시 오프셋을 준 상태 기준).
	// 내려가기 끝 자세와 매달리기 Idle은 루트 기준 손 높이가 달라서, 최종 매달림 높이는 이 값으로 정하고
	// 내려가는 동안은 끝 자세의 손이 모서리에 오도록 메시를 옮겼다가, 몽타주가 섞여 빠지는 만큼 Idle 위치로 옮긴다.
	// bDrawDebugDrop을 켜고 매달린 뒤 로그의 handZ-actor 값으로 잰다. 0이면 끝 자세와 Idle의 손 높이가 같다고 본다.
	// HoldMontage를 쓰면 끝 자세를 붙잡고 있으므로 무시한다.
	UPROPERTY(EditDefaultsOnly, Category = "LedgeHang|Hand", meta = (EditCondition = "bAlignHandToLedge", ClampMin = "0.0", Units = "cm"))
	float HangIdleHandHeight = 51.f;

	// 내려갈 때 모서리 꼭짓점, 손 목표, 예상 손 위치, 매달림 캡슐을 그린다
	UPROPERTY(EditDefaultsOnly, Category = "LedgeHang|Debug")
	bool bDrawDebugDrop = false;

	UPROPERTY(EditDefaultsOnly, Category = "LedgeHang|Debug", meta = (EditCondition = "bDrawDebugDrop", ClampMin = "0.0", Units = "s"))
	float DebugDrawTime = 5.f;

private:
	// 정지 몽타주를 재생하고 멈춘다. 재생하면 true.
	bool PlayHoldPose(ACharacter* Player) const;

	FLedgePlan Plan;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> LoadedDropMontage;

	// 이동 태스크에 넘기는 경로 곡선(X: 진행 방향, Z: 위)
	UPROPERTY(Transient)
	TObjectPtr<UCurveVector> PathCurve;

	bool bMovementChanged = false;
	bool bReachedHang = false;

	// 두 구간 몽타주일 때 둘째 구간 시작 시간, 몽타주 끝 시간, 섞는 시간
	// 매달리기 자세 태그를 붙였는지 (종료 시 뗀다)
	bool bHangPoseTagAdded = false;

	bool bTwoSectionDrop = false;
	float SectionTime = 0.f;
	float DropEndTime = 0.f;
	float CrossfadeTime = 0.f;

	// 메시 옮기기 (손 맞추기)
	FVector BaseMeshRelativeLocation = FVector::ZeroVector;
	double MeshShiftStartTime = 0.0;
	float MeshShiftDuration = 0.f;
	// 새로 시작할 때 이전 메시 옮기기를 멈추기 위한 번호
	int32 MeshShiftSerial = 0;
};
