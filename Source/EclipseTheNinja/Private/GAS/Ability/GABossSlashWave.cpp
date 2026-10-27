#include "GAS/Ability/GABossSlashWave.h"

#include "AbilitySystemComponent.h"
#include "Character/BossEnemy.h"
#include "Character/Enemy.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GAS/Effects/GEBasicAttackDamage.h"
#include "Projectile/BossSlashWave.h"

UGABossSlashWave::UGABossSlashWave()
{
	// 원거리 패턴이라 너무 가까운 대상에게는 쓰지 않고, 근접 패턴보다 멀리서 고를 수 있게 한다.
	MinUseDistance = 300.0f;
	MaxUseDistance = 900.0f;
}

bool UGABossSlashWave::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags
) const
{
	if (!ProjectileClass)
	{
		return false;
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UGABossSlashWave::OnPatternHit(const FBossAttackHitArea& HitArea)
{
	AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!Enemy || !SourceASC || Enemy->IsDead() || !ProjectileClass)
	{
		return;
	}

	if (!FMath::IsFinite(Damage) || Damage <= 0.0f)
	{
		return;
	}

	// 보스 정면 기준으로 발사 위치를 잡는다. 횡스크롤 평면(Y)은 보스와 같게 맞춘다.
	FVector SpawnLocation = Enemy->GetActorTransform().TransformPosition(SpawnOffset);
	SpawnLocation.Y = Enemy->GetActorLocation().Y;

	const FVector Direction = GetLaunchDirection(Enemy, SpawnLocation);

	// 피해 Spec은 발사 시점에 만들어 검기에 넘긴다. 맞힌 쪽의 피격 처리가 보스를 공격자로 알 수 있게 Instigator를 넣는다.
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(Enemy);
	Context.AddInstigator(Enemy, Enemy);

	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
		UGEBasicAttackDamage::StaticClass(),
		GetAbilityLevel(),
		Context
	);

	if (!Spec.IsValid())
	{
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Damage")), Damage);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Enemy;
	SpawnParams.Instigator = Enemy;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UWorld* World = Enemy->GetWorld();
	ABossSlashWave* Wave = World->SpawnActor<ABossSlashWave>(
		ProjectileClass,
		FTransform(Direction.Rotation(), SpawnLocation),
		SpawnParams
	);

	if (!Wave)
	{
		return;
	}

	// 체크포인트 리스폰 시 날아가던 검기를 정리한다.
	if (UCheckpointSubsystem* CheckpointSubsystem = World->GetSubsystem<UCheckpointSubsystem>())
	{
		CheckpointSubsystem->RegisterRuntimeSpawnedActor(Wave);
	}

	Wave->InitializeSlashWave(
		SourceASC,
		Spec,
		Direction,
		ProjectileSpeed
	);

	// 보스의 공격 디버그 토글이 켜져 있으면 발사 위치·조준 방향을 그리고 검기 판정을 보이게 한다.
	const ABossEnemy* Boss = Cast<ABossEnemy>(Enemy);
	if (Boss && Boss->IsDrawDebugAttackEnabled())
	{
		Wave->SetDrawDebug(true);
		DrawDebugLaunch(
			World,
			Enemy,
			SpawnLocation,
			Direction,
			Boss->GetDebugAttackDrawDuration()
		);
	}
}

void UGABossSlashWave::DrawDebugLaunch(
	const UWorld* World,
	const AEnemy* Enemy,
	const FVector& SpawnLocation,
	const FVector& Direction,
	float Duration
) const
{
	// 발사 위치
	DrawDebugSphere(
		World,
		SpawnLocation,
		10.0f,
		8,
		FColor::Orange,
		false,
		Duration
	);

	// 실제로 날아가는 방향(MaxAimPitch 제한 적용 후). 길이는 1초 동안 날아가는 거리.
	DrawDebugDirectionalArrow(
		World,
		SpawnLocation,
		SpawnLocation + Direction * ProjectileSpeed,
		40.0f,
		FColor::Orange,
		false,
		Duration,
		0,
		2.0f
	);

	// 조준 대상의 위치. 화살표와 어긋나 있으면 위아래 각도 제한에 걸린 것이다.
	if (const AActor* Target = Enemy->GetCombatTarget())
	{
		DrawDebugLine(
			World,
			SpawnLocation,
			Target->GetActorLocation(),
			FColor::Cyan,
			false,
			Duration,
			0,
			1.0f
		);
	}
}

FVector UGABossSlashWave::GetLaunchDirection(const AActor* Avatar, const FVector& SpawnLocation) const
{
	const AEnemy* Enemy = Cast<AEnemy>(Avatar);
	const AActor* Target = Enemy ? Enemy->GetCombatTarget() : nullptr;
	if (!Target)
	{
		return Avatar->GetActorForwardVector();
	}

	// 좌우는 대상 쪽, 위아래 각도는 MaxAimPitch 안으로 제한한다. 횡스크롤이라 Y 성분은 쓰지 않는다.
	const FVector ToTarget = Target->GetActorLocation() - SpawnLocation;
	const float HorizontalSign = FMath::Abs(ToTarget.X) > KINDA_SMALL_NUMBER
		? FMath::Sign(ToTarget.X)
		: (Avatar->GetActorForwardVector().X >= 0.0f ? 1.0f : -1.0f);

	const float PitchDegrees = FMath::Clamp(
		FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Z, FMath::Abs(ToTarget.X))),
		-MaxAimPitch,
		MaxAimPitch
	);

	const float PitchRadians = FMath::DegreesToRadians(PitchDegrees);
	return FVector(
		HorizontalSign * FMath::Cos(PitchRadians),
		0.0f,
		FMath::Sin(PitchRadians)
	);
}
