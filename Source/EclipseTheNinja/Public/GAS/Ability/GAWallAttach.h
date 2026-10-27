

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GAWallAttach.generated.h"

/**
 * 벽 감지와 매달림 고정을 관리하고, 종료 시 메시 위치와 낙하 이동을 복원한다.
 * 매달린 동안 기본 공격은 제한하며 공중·아래 암살 시작 시 취소된다.
 */
UCLASS()
class ECLIPSETHENINJA_API UGAWallAttach : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGAWallAttach();

	static FGameplayTag GetWallAttachAbilityTag();
	static FGameplayTag GetWallAttachingTag();

	// 매달린 동안 메시에 더하는 오프셋 (X: 벽 쪽, Z: 위). 모서리에서 내려오는 동안 미리 맞춰 두는 데 쓴다.
	FVector GetHangMeshOffset() const { return FVector(AnimXOffset, 0.f, AnimZOffset); }

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

	inline bool GetIsClimbable() const { return bIsClimbable; }

protected:
	UFUNCTION()
	void OnCompleted();

	UFUNCTION()
	void OnInterrupted();

private:
	bool FindWall(const ACharacter* Player, FHitResult& OutHit, bool* bOutClimbable = nullptr) const;

	inline void SetClimbable(bool Value) { bIsClimbable = Value; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach")
	bool bCanAttachOnGround = false;

	// 벽을 찾을 거리
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach", meta = (ClampMin = "0.0", Units = "cm"))
	float WallSearchDist = 50.f;

	// 벽을 찾을때 사용될 캡슐의 반지름
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach", meta = (ClampMin = "0.0", Units = "cm"))
	float WallSearchRadius = 10.f;

	// 벽을 찾을때 사용될 캡슐의 높이
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach", meta = (ClampMin = "0.0", Units = "cm"))
	float WallSearchHalfHeight = 35.f;

	// 벽에 붙은 동안 메시를 벽 쪽으로 옮길 거리 (캡슐은 벽에 딱 붙은 위치에 둔다)
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach", meta = (ClampMin = "-37", Units = "cm"))
	float AnimXOffset = 15.f;

	// 벽에 붙은 동안 메시를 위로 옮길 거리 (매달리기 애니메이션이 캡슐 아래로 내려가는 만큼)
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach", meta = (Units = "cm"))
	float AnimZOffset = 0.f;

	// 매달릴 수 있는 벽의 경사
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach", meta = (ClampMin = "0.0"))
	float MaxWallNormalZ = 0.1f;

	// 매달릴 수 있는 벽의 깊이 차이의 최대치
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach", meta = (ClampMin = "0.0"))
	float MaxWallDepthDiff = 5.f;

	// 벽에 매달릴 때 바닥과 떨어져야할 최소 높이
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach", meta = (ClampMin = "0.0"))
	float MinHeightAboveGround = 40.f;

	// 메시 오프셋을 주고 되돌리는 시간. 이전 몽타주(모서리 내려가기 등)가 섞여 빠지는 동안 메시가 튀지 않게 한다. 0이면 즉시.
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach", meta = (ClampMin = "0.0", Units = "s"))
	float MeshOffsetBlendTime = 0.25f;

	// 벽타기 가능 판정 트레이스를 화면에 그린다
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach|Debug")
	bool bDrawDebugClimbable = false;

private:
	// 메시 상대 위치를 Target까지 MeshOffsetBlendTime 동안 옮긴다. 어빌리티가 끝난 뒤에도 끝까지 이어진다.
	void BlendMeshTo(ACharacter* Player, const FVector& Target);
	void TickMeshBlend();

	// 캐릭터 클래스 기본 메시 상대 위치 (종료 시 복원)
	FVector BaseMeshRelativeLocation = FVector::ZeroVector;
	bool bMeshOffsetApplied = false;

	TWeakObjectPtr<ACharacter> BlendCharacter;
	FVector MeshBlendFrom = FVector::ZeroVector;
	FVector MeshBlendTo = FVector::ZeroVector;
	double MeshBlendStartTime = 0.0;
	FTimerHandle MeshBlendTimer;
	// 모서리에서 내려와 GALedgeHang의 정지 자세 몽타주로 매달려 있는지
	bool bHoldingLedgePose = false;

	// 매달린 동안 올라가기(GALedgeClimb) 디버그를 그린다. 타이머는 EndAbility에서 엔진이 지운다.
	void TickClimbDebug();
	FTimerHandle ClimbDebugTimer;

	bool bIsClimbable = false;


};
