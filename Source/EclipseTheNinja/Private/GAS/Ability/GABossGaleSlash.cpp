#include "GAS/Ability/GABossGaleSlash.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Character/Enemy.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

void UGABossGaleSlash::OnPatternHit(const FBossAttackHitArea& HitArea)
{
	AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	if (!IsActive() || !Enemy || Enemy->IsDead())
	{
		return;
	}

	// 돌진은 첫 타격 노티파이에서 한 번만 시작한다.
	if (bDashStarted)
	{
		return;
	}

	bDashStarted = true;
	bDashing = true;
	bDashHitTarget = false;
	DashHitArea = HitArea;
	DashPrevLocation = Enemy->GetActorLocation();

	// 플레이어를 뚫고 지나가도록 돌진하는 동안 캡슐이 Pawn과 부딪히지 않게 한다.
	if (bPassThroughPawns)
	{
		UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
		SavedPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		bPawnCollisionChanged = true;
	}

	// 시작 위치에 이미 겹쳐 있는 대상도 맞도록 출발 전에 한 번 검사한다.
	SweepDashPath();

	// 보스가 바라보는 방향으로 DashDistance만큼 이동한다. 끝나면 미끄러지지 않도록 속도를 0으로 둔다.
	const FVector DashDirection = Enemy->GetActorForwardVector().GetSafeNormal2D();
	const FVector TargetLocation = DashPrevLocation + DashDirection * DashDistance;

	UAbilityTask_ApplyRootMotionMoveToForce* DashTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this,
		NAME_None,
		TargetLocation,
		FMath::Max(0.01f, DashDuration),
		false,
		MOVE_Walking,
		false,
		nullptr,
		ERootMotionFinishVelocityMode::SetVelocity,
		FVector::ZeroVector,
		0.0f
	);
	DashTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UGABossGaleSlash::OnDashFinished);
	DashTask->OnTimedOut.AddDynamic(this, &UGABossGaleSlash::OnDashFinished);
	DashTask->ReadyForActivation();

	DashTickTimer = Enemy->GetWorldTimerManager().SetTimerForNextTick(this, &UGABossGaleSlash::TickDash);
}

void UGABossGaleSlash::TickDash()
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsActive() || !bDashing || !Avatar)
	{
		return;
	}

	SweepDashPath();

	DashTickTimer = Avatar->GetWorldTimerManager().SetTimerForNextTick(this, &UGABossGaleSlash::TickDash);
}

void UGABossGaleSlash::SweepDashPath()
{
	AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!Enemy || !SourceASC)
	{
		return;
	}

	// 판정 상자는 보스를 따라 움직이므로, 지난 위치의 상자에서 지금 위치의 상자까지 쓴다.
	const FTransform& ActorTransform = Enemy->GetActorTransform();
	const FVector WorldOffset = ActorTransform.TransformVector(DashHitArea.Offset);
	const FVector CurrentLocation = Enemy->GetActorLocation();
	const FVector SweepStart = DashPrevLocation + WorldOffset;
	const FVector SweepEnd = CurrentLocation + WorldOffset;
	DashPrevLocation = CurrentLocation;

	TArray<FHitResult> Hits;
	if (FVector::DistSquared(SweepStart, SweepEnd) < KINDA_SMALL_NUMBER)
	{
		// 움직이지 않은 프레임은 그 자리의 상자만 검사한다.
		QueryHitArea(Enemy, DashHitArea, Hits);
	}
	else
	{
		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossGaleSlash), false, Enemy);

		Enemy->GetWorld()->SweepMultiByObjectType(
			Hits,
			SweepStart,
			SweepEnd,
			ActorTransform.GetRotation(),
			ObjectParams,
			FCollisionShape::MakeBox(GetScaledHitExtent(ActorTransform, DashHitArea)),
			QueryParams
		);
	}

	// 돌진 한 번에 전투 대상은 한 번만 맞는다. 적끼리는 공격하지 않는다.
	AActor* CombatTarget = Enemy->GetCombatTarget();
	const float HitDamage = Damage * DashHitArea.DamageScale;
	if (!bDashHitTarget && CombatTarget && FMath::IsFinite(HitDamage) && HitDamage > 0.0f)
	{
		for (const FHitResult& Hit : Hits)
		{
			if (Hit.GetActor() != CombatTarget)
			{
				continue;
			}

			bDashHitTarget = ApplyHitDamage(SourceASC, CombatTarget, Hit, HitDamage);
			break;
		}
	}

	// 디버그를 켜면 프레임마다 지금 위치의 상자가 남아 돌진 경로가 보인다.
	DrawDebugHitArea(Enemy, DashHitArea, Hits, bDashHitTarget);
}

void UGABossGaleSlash::OnDashFinished()
{
	if (!IsActive() || !bDashing)
	{
		return;
	}

	// 마지막 프레임의 이동 구간까지 검사한 뒤 돌진을 끝낸다.
	SweepDashPath();
	bDashing = false;

	const AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	if (Enemy)
	{
		Enemy->GetWorldTimerManager().ClearTimer(DashTickTimer);
	}

	RestorePawnCollision();

	// 후딜레이: 베고 난 자세로 잠시 멈춘 뒤 몽타주의 남은 부분(평상 자세로 돌아오는 동작)을 재생한다.
	UAnimInstance* AnimInstance = Enemy ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	if (DashEndHoldTime <= 0.0f || !AnimInstance || !AnimInstance->Montage_IsPlaying(PatternMontage))
	{
		return;
	}

	AnimInstance->Montage_Pause(PatternMontage);

	UAbilityTask_WaitDelay* HoldTask = UAbilityTask_WaitDelay::WaitDelay(
		this,
		DashEndHoldTime
	);
	HoldTask->OnFinish.AddDynamic(this, &UGABossGaleSlash::OnDashEndHoldFinished);
	HoldTask->ReadyForActivation();
}

void UGABossGaleSlash::OnDashEndHoldFinished()
{
	const AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	UAnimInstance* AnimInstance = Enemy ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	if (!IsActive() || !AnimInstance)
	{
		return;
	}

	AnimInstance->Montage_Resume(PatternMontage);
}

void UGABossGaleSlash::RestorePawnCollision()
{
	if (!bPawnCollisionChanged)
	{
		return;
	}

	bPawnCollisionChanged = false;

	const AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	if (Enemy)
	{
		Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse);
	}
}

void UGABossGaleSlash::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	// 돌진 도중 패턴이 끊겨도 충돌과 타이머가 남지 않게 정리한다.
	const AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	if (Enemy)
	{
		Enemy->GetWorldTimerManager().ClearTimer(DashTickTimer);

		if (bDashing)
		{
			Enemy->GetCharacterMovement()->StopMovementImmediately();
		}
	}

	RestorePawnCollision();
	bDashStarted = false;
	bDashing = false;
	bDashHitTarget = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
