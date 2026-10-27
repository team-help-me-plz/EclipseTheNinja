#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GABossPatternBase.h"
#include "GABossSlashWave.generated.h"

class ABossSlashWave;

/**
 * 보스 원거리 발도 패턴. 발도하며 베는 순간 전투 대상 방향으로 검기 투사체를 날린다.
 * 원거리 적의 표창 투척처럼 타격 시점(Basic Attack Hit 노티파이 또는 HitTimes)마다 검기를 한 발씩 날린다.
 * 방향은 발사 순간의 대상 위치로 정하며, 위아래 각도는 MaxAimPitch로 제한한다.
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

protected:
	// 검기를 만들어 대상 방향으로 날린다.
	virtual void OnPatternHit() override;

	// 발사 위치에서 대상까지의 방향. 대상이 없으면 보스 정면으로 날린다.
	FVector GetLaunchDirection(const AActor* Avatar, const FVector& SpawnLocation) const;

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
};
