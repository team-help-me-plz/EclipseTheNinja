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
	DashHitArea = HitArea;
	DashStartLocation = Enemy->GetActorLocation();

	// 플레이어를 뚫고 지나가도록 돌진하는 동안 캡슐이 Pawn과 부딪히지 않게 한다.
	if (bPassThroughPawns)
	{
		UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
		SavedPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		bPawnCollisionChanged = true;
	}

	// 보스가 바라보는 방향으로 DashDistance만큼 이동한다. 끝나면 미끄러지지 않도록 속도를 0으로 둔다.
	const FVector DashDirection = Enemy->GetActorForwardVector().GetSafeNormal2D();
	const FVector TargetLocation = DashStartLocation + DashDirection * DashDistance;

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
}

FBossAttackHitArea UGABossGaleSlash::MakeDashPathArea(const AEnemy* Enemy) const
{
	// 실제로 이동한 거리를 보스 기준(회전·스케일을 뺀) 좌표로 바꾼다. 벽에 막히면 막힌 곳까지만 덮는다.
	const FTransform& ActorTransform = Enemy->GetActorTransform();
	const FVector LocalDelta = ActorTransform.InverseTransformVector(Enemy->GetActorLocation() - DashStartLocation);

	// 도착 위치 기준으로 출발 위치 상자까지 늘린다. 상자 중심은 이동한 거리의 절반만큼 뒤로 간다.
	FBossAttackHitArea PathArea = DashHitArea;
	PathArea.Offset = DashHitArea.Offset - LocalDelta * 0.5f;
	PathArea.HalfExtent = DashHitArea.HalfExtent + LocalDelta.GetAbs() * 0.5f;
	return PathArea;
}

void UGABossGaleSlash::ApplyDashPathDamage()
{
	AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!Enemy || !SourceASC || Enemy->IsDead())
	{
		return;
	}

	const FBossAttackHitArea PathArea = MakeDashPathArea(Enemy);

	TArray<FHitResult> Hits;
	QueryHitArea(Enemy, PathArea, Hits);

	// 경로 안의 전투 대상에게만 한 번 피해를 준다. 적끼리는 공격하지 않는다.
	AActor* CombatTarget = Enemy->GetCombatTarget();
	const float HitDamage = Damage * DashHitArea.DamageScale;
	bool bHitTarget = false;

	if (CombatTarget && FMath::IsFinite(HitDamage) && HitDamage > 0.0f)
	{
		for (const FHitResult& Hit : Hits)
		{
			if (Hit.GetActor() != CombatTarget)
			{
				continue;
			}

			bHitTarget = ApplyHitDamage(SourceASC, CombatTarget, Hit, HitDamage);
			break;
		}
	}

	// 디버그를 켜면 출발~도착 위치를 덮는 판정 상자 하나가 그려진다.
	DrawDebugHitArea(Enemy, PathArea, Hits, bHitTarget);
}

void UGABossGaleSlash::OnDashFinished()
{
	if (!IsActive() || !bDashing)
	{
		return;
	}

	// 다 지나간 뒤에 출발~도착 사이를 한 번 판정한다.
	bDashing = false;
	ApplyDashPathDamage();

	const AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());

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
	// 돌진 도중 패턴이 끊기면 판정 없이 멈추고, 바꾼 충돌을 되돌린다.
	const AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	if (Enemy && bDashing)
	{
		Enemy->GetCharacterMovement()->StopMovementImmediately();
	}

	RestorePawnCollision();
	bDashStarted = false;
	bDashing = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
