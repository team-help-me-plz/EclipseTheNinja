

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
		float OutDir = 1.f;	};

	// 바라보는 방향을 먼저, 그 다음 등 뒤 방향의 모서리를 찾는다.
	bool FindLedge(const ACharacter* Player, FLedgePlan& OutPlan) const;
	bool FindLedgeInDirection(const ACharacter* Player, float OutDir, FLedgePlan& OutPlan) const;

	// 모서리 윗면에서 매달린 캡슐 중심까지의 깊이.
	// 내려가기 몽타주의 루트 모션이 내려간 높이를 쓰면 몽타주 끝 자세의 손이 모서리에 맞는다.
	float ResolveHangDepth(float CapsuleHalfHeight) const;

	// 매달리기(GAWallAttach)가 메시에 더할 높이 오프셋. 정지 자세를 쓰거나 매달리기가 없으면 0.
	float GetHangMeshZOffset(const ACharacter* Player) const;

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
};
