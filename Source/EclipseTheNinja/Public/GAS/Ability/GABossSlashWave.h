#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GABossPatternBase.h"
#include "GABossSlashWave.generated.h"

class ABossSlashWave;
class AEnemy;

/**
 * 보스 원거리 발도 패턴. 발도하며 베는 순간 전투 대상 방향으로 검기 투사체를 날린다.
 * 원거리 적의 표창 투척처럼 몽타주의 Boss Attack Hit 노티파이마다 검기를 한 발씩 날린다.
 * 방향은 시전을 시작하는 순간(몽타주 재생 시점)의 대상 위치로 정해 고정한다. 그 뒤 대상이 움직여도 따라가지 않아
 * 피격 범위 표시와 실제 궤적이 같고, 플레이어가 보고 피할 수 있다. 위아래 각도는 MaxAimPitch로 제한한다.
 * 사용 거리는 UGABossPatternBase의 설정을, 쿨타임은 DT_BossPatterns 행의 Cooldown을 따른다.
 */
UCLASS()
class ECLIPSETHENINJA_API UGABossSlashWave : public UGABossPatternBase
{
	GENERATED_BODY()

public:
	UGABossSlashWave();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr
	) const override;

	/** 몽타주를 재생하기 전에 지금 대상 위치로 조준 방향을 정해 둔다. */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

protected:
	// 검기를 만들어 시전 시작 때 정한 방향으로 날린다. 노티파이의 판정 영역은 쓰지 않는다.
	virtual void OnPatternHit(const FBossAttackHitArea& HitArea) override;

	// 시전 시작 때 정한 조준 방향으로 발사 위치에서 TelegraphLength만큼 뻗은 띠를 범위로 보여 준다.
	virtual bool GetTelegraphArea(const AEnemy* Enemy, FBossTelegraphArea& OutArea) const override;

	// 발사 방향이 월드 기준으로 고정되므로, 보스가 도중에 몸을 돌려도 범위 표시는 돌지 않게 한다.
	virtual bool ShouldTelegraphFollowBoss() const override { return false; }

	// 보스 정면 기준 발사 위치. 횡스크롤 평면(Y)은 보스와 같게 맞춘다.
	FVector GetSpawnLocation(const AEnemy* Enemy) const;

	// 발사 위치에서 대상까지의 방향. 대상이 없으면 보스 정면으로 날린다.
	FVector GetLaunchDirection(const AActor* Avatar, const FVector& SpawnLocation) const;

	// 발사 위치, 실제 비행 방향, 조준 대상까지의 선을 그린다.
	void DrawDebugLaunch(
		const UWorld* World,
		const AEnemy* Enemy,
		const FVector& SpawnLocation,
		const FVector& Direction,
		float Duration
	) const;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Projectile")
	TSubclassOf<ABossSlashWave> ProjectileClass;

	/** 검기 한 발의 피해량. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Projectile", meta = (ClampMin = "0.0"))
	float Damage = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Projectile", meta = (ClampMin = "1.0", Units = "cm/s"))
	float ProjectileSpeed = 1400.0f;

	/** 보스 기준 발사 위치. X는 정면, Z는 위쪽(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Projectile", meta = (Units = "cm"))
	FVector SpawnOffset = FVector(80.0f, 0.0f, 20.0f);

	/** 수평선에서 위아래로 조준할 수 있는 최대 각도. 0이면 항상 수평으로 날린다. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Projectile", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float MaxAimPitch = 30.0f;

	/** 피격 범위 표시 띠의 길이(cm). 검기가 날아가는 거리에 맞춘다. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Telegraph", meta = (ClampMin = "1.0", Units = "cm"))
	float TelegraphLength = 1200.0f;

	/** 피격 범위 표시 띠 두께의 절반(cm). 검기 HitBox 높이의 절반에 맞춘다. */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Telegraph", meta = (ClampMin = "1.0", Units = "cm"))
	float TelegraphHalfHeight = 50.0f;

	// 시전 시작 때 정한 발사 방향(월드). 이번 시전의 모든 검기가 이 방향으로 날아간다.
	FVector LockedLaunchDirection = FVector::ForwardVector;
};
