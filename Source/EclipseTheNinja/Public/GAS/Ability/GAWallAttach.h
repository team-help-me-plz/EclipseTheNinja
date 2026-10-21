

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
	UFUNCTION()
	void OnCompleted();

	UFUNCTION()
	void OnInterrupted();

private:
	bool FindWall(const ACharacter* Player, FHitResult& OutHit) const;

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
	UPROPERTY(EditDefaultsOnly, Category = "WallAttach", meta = (ClampMin = "0.0", Units = "cm"))
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

private:
	// 매달리기 전 메시 상대 위치 (종료 시 복원)
	FVector BaseMeshRelativeLocation = FVector::ZeroVector;
	bool bMeshOffsetApplied = false;
};
