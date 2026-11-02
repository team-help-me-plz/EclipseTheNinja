#include "GAS/Ability/GABossPatternBase.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/BossAttackTelegraph.h"
#include "Character/Enemy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Animation/AnimNotify_BossAttackHit.h"

UGABossPatternBase::UGABossPatternBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	// 기본 공격과 같은 공격 중 태그를 사용해 패턴끼리 겹쳐 실행되지 않게 한다.
	ActivationOwnedTags.AddTag(UGABaseAttack::GetAttackingTag());
	ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());

	TelegraphClass = ABossAttackTelegraph::StaticClass();
}

bool UGABossPatternBase::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags
) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 몽타주와 재생할 애님 인스턴스가 있어야 한다.
	const AEnemy* Enemy = ActorInfo ? Cast<AEnemy>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Enemy || !PatternMontage)
	{
		return false;
	}

	if (!Enemy->GetMesh()->GetAnimInstance())
	{
		return false;
	}

	// 살아 있고, 공격 상태로 전환되었고, 공격할 대상이 있어야 한다.
	if (Enemy->IsDead() || Enemy->GetBehaviorState() != EEnemyBehaviorState::Attack)
	{
		return false;
	}

	return Enemy->GetCombatTarget() != nullptr;
}

void UGABossPatternBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	// 몽타주 앞부분에 노티파이가 있을 수 있으므로 재생보다 먼저 이벤트 대기를 건다.
	UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		UGABaseAttack::GetHitEventTag()
	);
	HitTask->EventReceived.AddDynamic(this, &UGABossPatternBase::OnHitEvent);
	HitTask->ReadyForActivation();

	// 준비 구간(몽타주 시작~준비 끝 위치)은 끝에서 멈추거나 느리게 재생해 늘이고, 그 뒤부터 원래 속도로 시전한다.
	const float BasePlayRate = FMath::Max(0.01f, PlayRate);
	const float WindupEnd = GetWindupEndPosition();
	const float NormalWindupDuration = WindupEnd / BasePlayRate;
	const float SlowRate = FMath::Clamp(WindupSlowRate, 0.05f, 1.0f);
	const bool bHoldWindup = bHoldWindupPose && WindupExtraTime > 0.0f;
	const bool bStretchWindup = !bHoldWindupPose && WindupEnd > 0.0f && SlowRate < 1.0f;
	const float StartPlayRate = bStretchWindup ? BasePlayRate * SlowRate : BasePlayRate;
	const float StretchedWindupDuration = NormalWindupDuration / SlowRate;

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		PatternMontage,
		StartPlayRate
	);
	MontageTask->OnCompleted.AddDynamic(this, &UGABossPatternBase::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UGABossPatternBase::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGABossPatternBase::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UGABossPatternBase::OnMontageInterrupted);
	MontageTask->ReadyForActivation();

	// 시전을 시작하는 순간 피격 범위를 보여 준다. 준비 구간 동안 플레이어가 피할 곳을 판단할 수 있다.
	if (IsActive())
	{
		ShowTelegraph();
	}

	if (bHoldWindup)
	{
		UAbilityTask_WaitDelay* HoldTask = UAbilityTask_WaitDelay::WaitDelay(
			this,
			NormalWindupDuration
		);
		HoldTask->OnFinish.AddDynamic(this, &UGABossPatternBase::OnWindupHoldReached);
		HoldTask->ReadyForActivation();
	}
	else if (bStretchWindup)
	{
		UAbilityTask_WaitDelay* WindupTask = UAbilityTask_WaitDelay::WaitDelay(
			this,
			StretchedWindupDuration
		);
		WindupTask->OnFinish.AddDynamic(this, &UGABossPatternBase::OnWindupFinished);
		WindupTask->ReadyForActivation();
	}
}

void UGABossPatternBase::OnWindupHoldReached()
{
	const AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	UAnimInstance* AnimInstance = Enemy ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	if (!IsActive() || !AnimInstance || !AnimInstance->Montage_IsPlaying(PatternMontage))
	{
		return;
	}

	// 대기 타이머와 실제 재생 위치가 프레임 단위로 어긋날 수 있어 준비 끝 위치로 맞춘 뒤 멈춘다.
	AnimInstance->Montage_SetPosition(PatternMontage, GetWindupEndPosition());
	AnimInstance->Montage_Pause(PatternMontage);

	UAbilityTask_WaitDelay* WindupTask = UAbilityTask_WaitDelay::WaitDelay(
		this,
		WindupExtraTime
	);
	WindupTask->OnFinish.AddDynamic(this, &UGABossPatternBase::OnWindupFinished);
	WindupTask->ReadyForActivation();
}

float UGABossPatternBase::GetWindupEndPosition() const
{
	if (!PatternMontage)
	{
		return 0.0f;
	}

	const float MontageLength = PatternMontage->GetPlayLength();
	if (WindupEndPosition > 0.0f)
	{
		return FMath::Min(WindupEndPosition, MontageLength);
	}

	// 따로 정하지 않으면 첫 타격 노티파이 앞부분의 AutoWindupEndRatio만큼을 준비 구간으로 본다.
	float FirstHitTime = MontageLength;
	for (const FAnimNotifyEvent& NotifyEvent : PatternMontage->Notifies)
	{
		if (Cast<UAnimNotify_BossAttackHit>(NotifyEvent.Notify))
		{
			FirstHitTime = FMath::Min(FirstHitTime, NotifyEvent.GetTriggerTime());
		}
	}

	return FirstHitTime * AutoWindupEndRatio;
}

void UGABossPatternBase::OnWindupFinished()
{
	const AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	UAnimInstance* AnimInstance = Enemy ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	if (!IsActive() || !AnimInstance)
	{
		return;
	}

	// 멈춤 방식이면 다시 재생하고, 늘이기 방식이면 원래 속도로 되돌린다.
	AnimInstance->Montage_Resume(PatternMontage);
	AnimInstance->Montage_SetPlayRate(PatternMontage, FMath::Max(0.01f, PlayRate));
}

void UGABossPatternBase::OnPatternHit(const FBossAttackHitArea& HitArea)
{
}

void UGABossPatternBase::OnMontageCompleted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
	}
}

void UGABossPatternBase::OnMontageInterrupted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
	}
}

void UGABossPatternBase::OnHitEvent(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}

	// Boss Attack Hit 노티파이는 자신을 OptionalObject로 보내 판정 영역을 함께 전달한다.
	// 같은 이벤트 태그를 쓰는 Basic Attack Hit 노티파이는 보스 패턴에서 쓰지 않으므로 무시한다.
	const UAnimNotify_BossAttackHit* HitNotify = Cast<UAnimNotify_BossAttackHit>(Payload.OptionalObject.Get());
	if (!HitNotify)
	{
		return;
	}

	HitEventCount++;
	OnPatternHit(HitNotify->GetHitArea());

	// 마지막 타격이 나가면 범위 표시를 지운다.
	if (bHideTelegraphOnLastHit && HitEventCount >= TotalHitCount)
	{
		HideTelegraph();
	}
}

void UGABossPatternBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	// 패턴이 끊기거나 끝나면 남아 있는 범위 표시를 지운다.
	HideTelegraph();
	HitEventCount = 0;
	TotalHitCount = 0;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UGABossPatternBase::FindFirstHitArea(FBossAttackHitArea& OutHitArea) const
{
	if (!PatternMontage)
	{
		return false;
	}

	const UAnimNotify_BossAttackHit* FirstNotify = nullptr;
	float FirstTime = TNumericLimits<float>::Max();
	for (const FAnimNotifyEvent& NotifyEvent : PatternMontage->Notifies)
	{
		const UAnimNotify_BossAttackHit* HitNotify = Cast<UAnimNotify_BossAttackHit>(NotifyEvent.Notify);
		if (HitNotify && NotifyEvent.GetTriggerTime() < FirstTime)
		{
			FirstNotify = HitNotify;
			FirstTime = NotifyEvent.GetTriggerTime();
		}
	}

	if (!FirstNotify)
	{
		return false;
	}

	OutHitArea = FirstNotify->GetHitArea();
	return true;
}

bool UGABossPatternBase::GetTelegraphArea(const AEnemy* Enemy, FBossTelegraphArea& OutArea) const
{
	if (!PatternMontage)
	{
		return false;
	}

	// 타격마다 범위가 달라도 하나로 단순화해 보여 준다. 모든 판정 상자를 덮는 X-Z 사각형을 만든다.
	FBox2D Bounds(ForceInit);
	float CenterY = 0.0f;
	for (const FAnimNotifyEvent& NotifyEvent : PatternMontage->Notifies)
	{
		const UAnimNotify_BossAttackHit* HitNotify = Cast<UAnimNotify_BossAttackHit>(NotifyEvent.Notify);
		if (!HitNotify)
		{
			continue;
		}

		const FBossAttackHitArea& HitArea = HitNotify->GetHitArea();
		const FVector2D Center(HitArea.Offset.X, HitArea.Offset.Z);
		const FVector2D HalfSize(HitArea.HalfExtent.X, HitArea.HalfExtent.Z);

		Bounds += Center - HalfSize;
		Bounds += Center + HalfSize;
		CenterY = HitArea.Offset.Y;
	}

	if (!Bounds.bIsValid)
	{
		return false;
	}

	const FVector2D Center = Bounds.GetCenter();
	OutArea.Center = FVector(Center.X, CenterY, Center.Y);
	OutArea.HalfSize = Bounds.GetExtent();
	OutArea.PitchDegrees = 0.0f;
	return true;
}

void UGABossPatternBase::ShowTelegraph()
{
	// 몽타주의 전체 타격 수를 세어 두어 마지막 타격에서 표시를 지울 수 있게 한다.
	HitEventCount = 0;
	TotalHitCount = 0;
	if (PatternMontage)
	{
		for (const FAnimNotifyEvent& NotifyEvent : PatternMontage->Notifies)
		{
			if (Cast<UAnimNotify_BossAttackHit>(NotifyEvent.Notify))
			{
				TotalHitCount++;
			}
		}
	}

	HideTelegraph();

	AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	if (!bShowTelegraph || !TelegraphClass || !Enemy)
	{
		return;
	}

	FBossTelegraphArea Area;
	if (!GetTelegraphArea(Enemy, Area))
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Enemy;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ActiveTelegraph = Enemy->GetWorld()->SpawnActor<ABossAttackTelegraph>(
		TelegraphClass,
		Enemy->GetActorTransform(),
		SpawnParams
	);

	if (ActiveTelegraph)
	{
		ActiveTelegraph->Show(Enemy, Area, ShouldTelegraphFollowBoss());
	}
}

void UGABossPatternBase::HideTelegraph()
{
	if (IsValid(ActiveTelegraph))
	{
		ActiveTelegraph->Hide();
	}

	ActiveTelegraph = nullptr;
}
