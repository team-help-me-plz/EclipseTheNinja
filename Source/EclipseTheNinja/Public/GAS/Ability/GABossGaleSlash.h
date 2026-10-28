#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GABossPatternAttack.h"
#include "GAS/Animation/AnimNotify_BossAttackHit.h"
#include "GABossGaleSlash.generated.h"

/**
 * 보스 질풍참 패턴. 발도 준비 자세로 오래 기를 모은 뒤, 순식간에 정면으로 DashDistance만큼 지나가며 벤다.
 *
 * - 준비: UGABossPatternBase의 Windup 설정(WindupExtraTime 등)으로 준비 자세 길이를 정한다.
 * - 시전: 몽타주의 첫 Boss Attack Hit 노티파이에서 돌진을 시작한다. 돌진하는 동안 노티파이의 판정 상자를
 *   보스와 함께 이동 경로를 따라 쓸고 지나가며, 경로에 걸린 전투 대상에게 한 번만 피해를 준다.
 *   돌진 중에는 Pawn과 부딪히지 않아 플레이어를 뚫고 지나간다. 벽에 막히면 그 자리에서 멈춘다.
 * - 후딜레이: 돌진이 끝나면 DashEndHoldTime만큼 베고 난 자세로 멈춘 뒤, 몽타주의 남은 부분(평상 자세로 돌아오는 동작)을 재생한다.
 *
 * 시전 순간의 반짝임 같은 이펙트는 몽타주에 엔진의 Play Niagara Particle Effect 노티파이로 넣는다.
 */
UCLASS()
class ECLIPSETHENINJA_API UGABossGaleSlash : public UGABossPatternAttack
{
	GENERATED_BODY()

public:
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

protected:
	// 첫 타격 노티파이에서 돌진을 시작한다. 이후 노티파이는 무시한다.
	virtual void OnPatternHit(const FBossAttackHitArea& HitArea) override;

	// 매 프레임 지난 위치에서 지금 위치까지 판정 상자를 쓸어 대상을 맞힌다.
	void TickDash();

	// 판정 상자를 이전 보스 위치에서 지금 보스 위치까지 쓸어 전투 대상에게 피해를 준다.
	void SweepDashPath();

	// 돌진이 끝나면(목표 도달 또는 벽에 막혀 시간 초과) 충돌을 되돌리고 후딜레이를 시작한다.
	UFUNCTION()
	void OnDashFinished();

	// 후딜레이 자세 유지가 끝나면 몽타주의 남은 부분을 재생한다.
	UFUNCTION()
	void OnDashEndHoldFinished();

	// 돌진 중 Pawn을 통과하도록 바꾼 캡슐 충돌을 원래대로 되돌린다.
	void RestorePawnCollision();

	/** 돌진 거리(cm). 보스 액터 스케일과 상관없이 이 거리만큼 정면으로 이동한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Dash", meta = (ClampMin = "0.0", Units = "cm"))
	float DashDistance = 900.0f;

	/** 돌진에 걸리는 시간(초). 짧을수록 순간이동처럼 보인다. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Dash", meta = (ClampMin = "0.01", Units = "s"))
	float DashDuration = 0.12f;

	/** 돌진 중 플레이어 등 Pawn을 뚫고 지나간다. false면 Pawn에 막힌다. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Dash")
	bool bPassThroughPawns = true;

	/**
	 * 돌진이 끝난 뒤 베고 난 자세로 멈춰 있는 시간(초). 몽타주의 남은 부분(평상 자세로 돌아오는 동작) 앞에 더해지는 후딜레이다.
	 * 0이면 멈추지 않고 바로 남은 부분을 재생한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Recovery", meta = (ClampMin = "0.0", Units = "s"))
	float DashEndHoldTime = 0.6f;

	// 돌진 상태
	bool bDashStarted = false;
	bool bDashing = false;
	bool bDashHitTarget = false;
	FBossAttackHitArea DashHitArea;
	FVector DashPrevLocation = FVector::ZeroVector;
	FTimerHandle DashTickTimer;

	// 돌진 전 캡슐의 Pawn 채널 응답. 돌진이 끝나면 되돌린다.
	bool bPawnCollisionChanged = false;
	TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;
};
