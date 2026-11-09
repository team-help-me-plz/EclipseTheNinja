#include "GAS/Ability/GABossExecution.h"

#include "AIController.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BrainComponent.h"
#include "Character/BossEnemy.h"
#include "Character/PlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Ability/GAAssassinateBase.h"
#include "GAS/Animation/AnimNotify_AssassinateImpact.h"
#include "GAS/Task/AbilityTask_MontageRootOffset.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

UGABossExecution::UGABossExecution()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	// 보스가 이 C++ 클래스를 그대로 쓰므로 기본 피 이펙트를 여기서 지정한다.
	VictimBloodSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(
		TEXT("/Game/VFX/Vefects/Blood_VFX/VFX/Performance_Versions/Bursts/Looped/Looped_NS_Blood_Burst_Large.Looped_NS_Blood_Burst_Large")));
	VictimBloodSlashSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(
		TEXT("/Game/VFX/Vefects/Blood_VFX/VFX/Performance_Versions/Slashes/Looped/Looped_NS_Slash_Large.Looped_NS_Slash_Large")));
}

void UGABossExecution::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	bIgnoringVictimCollision = false;
	bImpactTriggered = false;
	bVictimBloodSpawned = false;
	PausedBrain.Reset();

	ABossEnemy* Boss = Cast<ABossEnemy>(ActorInfo->AvatarActor.Get());
	APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(Boss, 0));
	if (!IsValid(Boss) || !IsValid(Player) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	Victim = Player;

	// 연출 동안 보스 AI와 이동을 멈춘다.
	if (AAIController* AI = Cast<AAIController>(Boss->GetController()))
	{
		AI->StopMovement();
		UBrainComponent* Brain = AI->GetBrainComponent();
		if (IsValid(Brain) && !Brain->IsPaused())
		{
			Brain->PauseLogic(TEXT("BossExecution"));
			PausedBrain = Brain;
		}
	}

	Boss->GetCharacterMovement()->StopMovementImmediately();

	// 플레이어가 공중에서 처형되면 보스도 그 옆 공중에 서야 하므로 연출 동안 중력을 끈다.
	// 플레이어는 사망 처리에서 이동이 꺼져(MOVE_None) 그 자리에 멈춘다.
	SavedBossMovementMode = Boss->GetCharacterMovement()->MovementMode;
	Boss->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	bBossGravityDisabled = true;

	// 연출이 끝나면 이 자리로 되돌린다
	BossStartTransform = Boss->GetActorTransform();
	bReturnBossOnEnd = false;

	MoveBehindVictim(Boss, Player);
	SetIgnoreVictimCollision(true);

	ExecutionAttackMontage = Boss->GetExecutionAttackMontage().LoadSynchronous();
	UAnimMontage* VictimMontage = Boss->GetExecutionVictimMontage().LoadSynchronous();

	// 보스 암살자 몽타주. 스켈레톤이 맞지 않으면 재생되지 않으므로 경고만 남기고 플레이어 연출은 그대로 진행한다.
	float AttackDuration = 0.0f;
	UAnimInstance* BossAnimInstance = Boss->GetMesh()->GetAnimInstance();
	if (IsValid(ExecutionAttackMontage) && BossAnimInstance)
	{
		// 노티파이는 몽타주가 틱할 때 오므로 재생 전에 듣기 시작한다.
		WaitForImpact();
		AttackDuration = BossAnimInstance->Montage_Play(ExecutionAttackMontage, 1.0f, EMontagePlayReturnType::Duration);

		// 끝나기 전에 자동으로 블렌드 아웃되면 납도 마지막 동작이 대기 자세(칼을 든 자세)와 섞여 끝까지 넣지 않은 것처럼 보인다.
		// 납도를 마친 마지막 자세로 멈춰 두고, 화면이 어두워진 뒤 EndAbility에서 멈춘다.
		if (FAnimMontageInstance* MontageInstance = BossAnimInstance->GetActiveInstanceForMontage(ExecutionAttackMontage))
		{
			MontageInstance->bEnableAutoBlendOut = false;
		}
	}

	if (AttackDuration <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 처형 암살자 몽타주가 없거나 보스 스켈레톤에서 재생할 수 없습니다. (%s)"),
			*Boss->GetName(),
			*GetNameSafe(ExecutionAttackMontage));
	}

	// 피격자 몽타주로 기존 사망 처리를 실행한다. 몽타주가 없으면 일반 사망 몽타주를 쓴다.
	Player->ReceiveAssassination(VictimMontage);


	// 플레이어는 사망으로 이동이 꺼져 있으므로 쓰러지며 움직이는 루트 이동을 메시로 보여준다.
	float VictimDuration = 0.0f;
	if (IsValid(VictimMontage))
	{
		VictimDuration = VictimMontage->GetPlayLength();

		UAbilityTask_MontageRootOffset* OffsetTask = UAbilityTask_MontageRootOffset::FollowMontageRoot(
			this,
			Player,
			VictimMontage,
			false,
			true
		);
		OffsetTask->ReadyForActivation();
	}

	// 두 몽타주(보스는 납도까지)가 끝나고 쓰러진 장면을 더 보여 주는 동안 보스를 붙잡아 둔다.
	const float ExecutionDuration = FMath::Max3(AttackDuration, VictimDuration, 0.01f) + PostExecutionHoldTime;
	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, ExecutionDuration);

	// 사망 처리는 피격자 몽타주 길이만큼 지나면 리스폰으로 넘어간다. 보스의 납도가 끝나고 화면이 다 어두워질 때까지 늦춘다.
	// 처형 컷신은 화면이 다 어두워질 때 끝나며 카메라를 돌려준다. 그 뒤에 리스폰해야 컷신이 사라진 캐릭터로 카메라를 되돌리지 않는다.
	const float RespawnTime = ExecutionDuration + EndFadeOutTime + (EndFadeOutTime > 0.0f ? 0.1f : 0.0f);
	if (Player->GetLifeSpan() > 0.0f && RespawnTime > Player->GetLifeSpan())
	{
		Player->SetLifeSpan(RespawnTime);
	}
	WaitTask->OnFinish.AddDynamic(this, &UGABossExecution::OnExecutionFinished);
	WaitTask->ReadyForActivation();

	Boss->OnExecutionStarted.Broadcast();
}

void UGABossExecution::MoveBehindVictim(ABossEnemy* Boss, const APlayerCharacter* Player) const
{
	const UCapsuleComponent* BossCapsule = Boss->GetCapsuleComponent();
	const UCapsuleComponent* PlayerCapsule = Player->GetCapsuleComponent();

	// 캡슐이 겹치지 않는 거리보다 가깝게 서지 않는다.
	const float CapsuleSeparation = BossCapsule->GetScaledCapsuleRadius()
		+ PlayerCapsule->GetScaledCapsuleRadius()
		+ 5.0f;
	const float DistanceBehind = FMath::Max(Boss->GetExecutionBehindDistance(), CapsuleSeparation);

	// 플레이어 등 뒤에 서고, 횡스크롤 평면(Y)은 보스 것을 유지한다. 높이는 두 캡슐의 바닥을 맞춘다.
	const FVector PlayerForward = Player->GetActorForwardVector().GetSafeNormal2D();
	FVector Location = Player->GetActorLocation() - PlayerForward * DistanceBehind;
	Location.Y = Boss->GetActorLocation().Y;
	Location.Z = Player->GetActorLocation().Z
		- PlayerCapsule->GetScaledCapsuleHalfHeight()
		+ BossCapsule->GetScaledCapsuleHalfHeight();

	const FRotator Rotation(0.0f, Player->GetActorRotation().Yaw, 0.0f);

	// 벽에 겹치면 가까운 빈자리를 찾고, 그래도 안 되면 그대로 옮긴다.
	if (!Boss->TeleportTo(Location, Rotation))
	{
		Boss->SetActorLocationAndRotation(
			Location,
			Rotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);
	}

	// 보스의 서서히 도는 회전 목표도 같은 방향으로 맞춰 연출 중 돌아서지 않게 한다.
	Boss->FaceDirection(PlayerForward.X);
}

void UGABossExecution::SetIgnoreVictimCollision(bool bIgnore)
{
	if (bIgnoringVictimCollision == bIgnore)
	{
		return;
	}

	ACharacter* Boss = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	APlayerCharacter* Player = Victim.Get();
	if (IsValid(Boss) && IsValid(Player))
	{
		Boss->GetCapsuleComponent()->IgnoreActorWhenMoving(Player, bIgnore);
		Player->GetCapsuleComponent()->IgnoreActorWhenMoving(Boss, bIgnore);
	}

	bIgnoringVictimCollision = bIgnore;
}

void UGABossExecution::WaitForImpact()
{
	if (!bUseImpactTimeDilation || !IsActive())
	{
		return;
	}

	// 몽타주는 1배속으로 재생하므로, 몽타주 시작부터 잰 게임 시간 대기가 몽타주 시간과 같다.
	// 예비 동작 슬로모가 걸려도 대기 시간이 같이 늘어나 시점이 어긋나지 않는다.
	float ImpactTime = ImpactFallbackTime;
	if (FindImpactNotifyTime(ImpactTime))
	{
		// 노티파이는 몽타주가 틱할 때 오므로 재생 전에 듣기 시작한다.
		UAbilityTask_WaitGameplayEvent* ImpactTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			UGAAssassinateBase::GetImpactEventTag(),
			nullptr,
			true
		);
		ImpactTask->EventReceived.AddDynamic(this, &UGABossExecution::OnImpactEvent);
		ImpactTask->ReadyForActivation();
	}
	else if (ImpactFallbackTime >= 0.0f)
	{
		// 노티파이를 아직 안 찍은 몽타주도 확인할 수 있게 고정 시점으로 대신한다.
		UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, ImpactFallbackTime);
		DelayTask->OnFinish.AddDynamic(this, &UGABossExecution::OnImpactFallback);
		DelayTask->ReadyForActivation();
	}
	else
	{
		return;
	}

	// 예비 동작: 칼을 뻗기 전까지 느리게 보여 긴장을 만들고, 칼이 들어가는 구간은 원래 속도로 되돌려 빠르게 꽂는다.
	if (!bUseAnticipation || AnticipationLeadTime <= SnapLeadTime)
	{
		return;
	}

	// 플레이어 암살과 같이, 예비 동작 시작 시점이 몽타주 시작보다 앞이면 예비 동작 없이 진행한다.
	const float AnticipationStart = ImpactTime - AnticipationLeadTime;
	const float SnapTime = ImpactTime - SnapLeadTime;
	if (AnticipationStart <= KINDA_SMALL_NUMBER || SnapTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	UAbilityTask_WaitDelay* AnticipationTask = UAbilityTask_WaitDelay::WaitDelay(this, AnticipationStart);
	AnticipationTask->OnFinish.AddDynamic(this, &UGABossExecution::OnAnticipationStart);
	AnticipationTask->ReadyForActivation();

	UAbilityTask_WaitDelay* SnapTask = UAbilityTask_WaitDelay::WaitDelay(this, SnapTime);
	SnapTask->OnFinish.AddDynamic(this, &UGABossExecution::OnAnticipationSnap);
	SnapTask->ReadyForActivation();
}

void UGABossExecution::OnImpactEvent(FGameplayEventData Payload)
{
	SpawnVictimBlood();
	StartImpactTimeDilation();
}

void UGABossExecution::OnImpactFallback()
{
	SpawnVictimBlood();
	StartImpactTimeDilation();
}

void UGABossExecution::SpawnVictimBlood()
{
	APlayerCharacter* Player = Victim.Get();
	if (bVictimBloodSpawned || !IsValid(Player))
	{
		return;
	}
	bVictimBloodSpawned = true;

	UNiagaraSystem* System = VictimBloodSystem.LoadSynchronous();
	if (!System)
	{
		return;
	}

	// 피는 보스 칼이 플레이어 몸을 지나는 자리에서, 칼날이 나가는 방향으로 뿜어지게 한다.
	USkeletalMeshComponent* Mesh = Player->GetMesh();
	const bool bHasBone = Mesh && !VictimBloodBone.IsNone() && Mesh->DoesSocketExist(VictimBloodBone);
	const FVector BodyPoint = bHasBone ? Mesh->GetSocketLocation(VictimBloodBone) : Player->GetActorLocation();
	const ACharacter* Boss = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	FVector Location = BodyPoint;
	FVector Direction = IsValid(Boss)
		? (BodyPoint - Boss->GetActorLocation()).GetSafeNormal2D()
		: Player->GetActorForwardVector();
	const USkeletalMeshComponent* BossMesh = IsValid(Boss) ? Boss->GetMesh() : nullptr;
	if (BossMesh && !BossBladeBone.IsNone() && BossMesh->DoesSocketExist(BossBladeBone))
	{
		// 칼날은 칼 뼈에서 BossBladeAxis 방향으로 BossBladeLength만큼 뻗어 있다. 그 선분에서 플레이어 몸에 가장 가까운 점이 칼이 박힌 자리다.
		const FTransform BladeTransform = BossMesh->GetSocketTransform(BossBladeBone);
		const FVector BladeStart = BladeTransform.GetLocation();
		const FVector BladeDir = BladeTransform.TransformVectorNoScale(BossBladeAxis.GetSafeNormal());
		const FVector BladeEnd = BladeStart + BladeDir * BossBladeLength;
		Location = FMath::ClosestPointOnSegment(BodyPoint, BladeStart, BladeEnd);
		Direction = BladeDir;
	}

	// 반복 이펙트는 정해진 시간이 지나면 새로 뿜지 않게 끈다. 이미 나온 입자는 자연스럽게 사라진다.
	auto StopAfter = [Player](UNiagaraComponent* Comp, float ActiveTime)
	{
		if (!Comp || ActiveTime <= 0.0f)
		{
			return;
		}
		TWeakObjectPtr<UNiagaraComponent> WeakComp = Comp;
		FTimerHandle Handle;
		Player->GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Comp, [WeakComp]()
		{
			if (UNiagaraComponent* Alive = WeakComp.Get())
			{
				Alive->Deactivate();
			}
		}), ActiveTime, false);
	};

	// 칼이 박히는 순간: 크게 터지는 피와 베인 자리에서 튀는 피
	StopAfter(UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		Player, System, Location, Direction.Rotation(), FVector(VictimBloodScale), true, true, ENCPoolMethod::None, true),
		VictimBloodActiveTime);
	if (UNiagaraSystem* Slash = VictimBloodSlashSystem.LoadSynchronous())
	{
		StopAfter(UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			Player, Slash, Location, Direction.Rotation(), FVector(VictimBloodScale), true, true, ENCPoolMethod::None, true),
			VictimBloodActiveTime);
	}

	// 쓰러지는 동안 몸에서 피가 몇 번 더 뿜어져 나온다. 몸을 따라가도록 뼈에 붙인다.
	if (!Mesh || VictimBloodSpurtCount <= 0)
	{
		return;
	}
	const FName AttachBone = bHasBone ? VictimBloodBone : NAME_None;
	// 칼이 박힌 자리를 몸 뼈 기준으로 기억해 쓰러지는 동안에도 같은 자리에서 뿜어지게 한다
	const FVector AttachOffset = Mesh->GetSocketTransform(AttachBone).InverseTransformPosition(Location);
	const float SpurtScale = VictimBloodScale * VictimBloodSpurtScaleRatio;
	const float SpurtActiveTime = VictimBloodActiveTime * 0.6f;
	TWeakObjectPtr<USkeletalMeshComponent> WeakMesh = Mesh;
	TWeakObjectPtr<UNiagaraSystem> WeakSystem = System;
	for (int32 Index = 1; Index <= VictimBloodSpurtCount; ++Index)
	{
		FTimerHandle Handle;
		Player->GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Player, [WeakMesh, WeakSystem, AttachBone, AttachOffset, Direction, SpurtScale, SpurtActiveTime, StopAfter]()
		{
			USkeletalMeshComponent* AliveMesh = WeakMesh.Get();
			UNiagaraSystem* AliveSystem = WeakSystem.Get();
			if (!AliveMesh || !AliveSystem)
			{
				return;
			}
			// 매번 조금씩 다른 방향으로 뿜어 같은 이펙트가 반복돼 보이지 않게 한다
			FRotator Rotation = Direction.Rotation();
			Rotation.Yaw += FMath::FRandRange(-35.0f, 35.0f);
			Rotation.Pitch += FMath::FRandRange(-20.0f, 30.0f);
			UNiagaraComponent* Spurt = UNiagaraFunctionLibrary::SpawnSystemAttached(
				AliveSystem, AliveMesh, AttachBone, AttachOffset, Rotation,
				FVector(SpurtScale), EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::None, true);
			if (Spurt)
			{
				// 몸을 따라가되 뿜는 방향은 월드 기준으로 둔다
				Spurt->SetWorldRotation(Rotation);
			}
			StopAfter(Spurt, SpurtActiveTime);
		}), VictimBloodSpurtInterval * Index, false);
	}
}

void UGABossExecution::OnAnticipationStart()
{
	if (bImpactTriggered || !IsActive())
	{
		return;
	}

	// 한 번에 떨어뜨리면 끊겨 보이므로 티커에서 서서히 느려지게 한다.
	SetTimeScale(1.0f);
	TimeDilationPhase = ETimeDilationPhase::Anticipation;
	ImpactElapsed = 0.0f;
	StartTimeDilationTicker();
}

void UGABossExecution::OnAnticipationSnap()
{
	if (bImpactTriggered || !IsActive())
	{
		return;
	}

	// 칼이 들어가는 구간은 즉시 원래 속도로 돌려 빠르게 꽂히는 대비를 만든다.
	StopTimeDilationTicker();
	TimeDilationPhase = ETimeDilationPhase::None;
	SetTimeScale(1.0f);
}

bool UGABossExecution::FindImpactNotifyTime(float& OutTime) const
{
	if (!IsValid(ExecutionAttackMontage))
	{
		return false;
	}

	// 노티파이는 몽타주 자체에도, 몽타주가 담은 시퀀스에도 찍을 수 있다.
	for (const FAnimNotifyEvent& Event : ExecutionAttackMontage->Notifies)
	{
		if (Cast<UAnimNotify_AssassinateImpact>(Event.Notify))
		{
			OutTime = Event.GetTriggerTime();
			return true;
		}
	}

	for (const FSlotAnimationTrack& Slot : ExecutionAttackMontage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
		{
			const UAnimSequenceBase* Animation = Segment.GetAnimReference();
			if (!IsValid(Animation))
			{
				continue;
			}

			for (const FAnimNotifyEvent& Event : Animation->Notifies)
			{
				if (!Cast<UAnimNotify_AssassinateImpact>(Event.Notify))
				{
					continue;
				}

				// 시퀀스 안의 시간을 몽타주 시간으로 바꾼다. 구간 밖에 찍힌 노티파이는 재생되지 않는다.
				const float AnimTime = Event.GetTriggerTime();
				if (AnimTime < Segment.AnimStartTime || AnimTime > Segment.AnimEndTime)
				{
					continue;
				}

				const float PlayRate = FMath::Max(FMath::Abs(Segment.GetValidPlayRate()), KINDA_SMALL_NUMBER);
				OutTime = Segment.StartPos + (AnimTime - Segment.AnimStartTime) / PlayRate;
				return true;
			}
		}
	}

	return false;
}

void UGABossExecution::SetTimeScale(float Scale)
{
	// 이미 느려진 상태일 수 있으므로 처음 바꿀 때의 배율을 기준으로 곱하고 그대로 되돌린다.
	if (!bTimeDilationApplied)
	{
		SavedTimeDilation = UGameplayStatics::GetGlobalTimeDilation(this);
		bTimeDilationApplied = true;
	}

	UGameplayStatics::SetGlobalTimeDilation(this, SavedTimeDilation * Scale);
}

void UGABossExecution::StartImpactTimeDilation()
{
	if (bImpactTriggered || !IsActive())
	{
		return;
	}

	bImpactTriggered = true;
	TimeDilationPhase = ETimeDilationPhase::Impact;
	ImpactElapsed = 0.0f;
	SetTimeScale(HitStopTimeScale);
	StartTimeDilationTicker();
}

void UGABossExecution::StartTimeDilationTicker()
{
	if (ImpactTickerHandle.IsValid())
	{
		return;
	}

	ImpactTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UGABossExecution::TickImpactTimeDilation)
	);
}

void UGABossExecution::StopTimeDilationTicker()
{
	if (ImpactTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ImpactTickerHandle);
	}

	ImpactTickerHandle.Reset();
}

bool UGABossExecution::TickImpactTimeDilation(float DeltaTime)
{
	if (!bTimeDilationApplied || !GetWorld() || TimeDilationPhase == ETimeDilationPhase::None)
	{
		ImpactTickerHandle.Reset();
		return false;
	}

	// 모두 실제 시간 기준이다.
	ImpactElapsed += DeltaTime;

	if (TimeDilationPhase == ETimeDilationPhase::Anticipation)
	{
		// 예비 동작: 원래 속도에서 예비 동작 배율까지 부드럽게 감속한 뒤 유지한다. 끝은 OnAnticipationSnap이 맡는다.
		const float Alpha = FMath::Clamp(
			ImpactElapsed / FMath::Max(AnticipationBlendTime, KINDA_SMALL_NUMBER),
			0.0f,
			1.0f
		);
		UGameplayStatics::SetGlobalTimeDilation(
			this,
			SavedTimeDilation * FMath::InterpEaseOut(1.0f, AnticipationTimeScale, Alpha, 2.0f)
		);
		return true;
	}

	// 임팩트: 히트스톱 → 짧은 슬로모션 유지 → 시작과 끝이 부드러운 곡선으로 원래 속도 복귀.
	const float SlowStart = HitStopDuration;
	const float RecoverStart = SlowStart + SlowMotionDuration;
	const float RecoverEnd = RecoverStart + RecoverDuration;
	if (ImpactElapsed >= RecoverEnd)
	{
		// 반환값 false로 티커가 빠지므로 직접 제거하지 않는다.
		StopImpactTimeDilation(false);
		return false;
	}

	float Scale = SlowMotionTimeScale;
	if (ImpactElapsed < SlowStart)
	{
		Scale = HitStopTimeScale;
	}
	else if (ImpactElapsed >= RecoverStart)
	{
		const float Alpha = (ImpactElapsed - RecoverStart) / FMath::Max(RecoverDuration, KINDA_SMALL_NUMBER);
		Scale = FMath::InterpEaseInOut(SlowMotionTimeScale, 1.0f, Alpha, 2.0f);
	}

	UGameplayStatics::SetGlobalTimeDilation(this, SavedTimeDilation * Scale);
	return true;
}

void UGABossExecution::StopImpactTimeDilation(bool bRemoveTicker)
{
	if (bRemoveTicker)
	{
		StopTimeDilationTicker();
	}

	ImpactTickerHandle.Reset();
	TimeDilationPhase = ETimeDilationPhase::None;

	if (!bTimeDilationApplied)
	{
		return;
	}

	bTimeDilationApplied = false;
	if (GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(this, SavedTimeDilation);
	}
}

void UGABossExecution::OnExecutionFinished()
{
	// 납도까지 끝나면 화면을 어둡게 한 뒤 리스폰한다. 컷신이 끝나며 카메라가 바뀌고 체크포인트로 옮겨지는 순간을 가린다.
	// 리스폰한 캐릭터를 조종하기 시작할 때 컨트롤러가 다시 밝힌다.
	const APlayerCharacter* Player = Victim.Get();
	const APlayerController* PlayerController = IsValid(Player) ? Cast<APlayerController>(Player->GetController()) : nullptr;
	if (EndFadeOutTime > 0.0f && PlayerController && PlayerController->PlayerCameraManager)
	{
		PlayerController->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, EndFadeOutTime, FLinearColor::Black, false, true);

		// 화면이 다 어두워질 때까지 보스를 공중에 붙잡아 둔다. 끝낼 때 원래 자리로 되돌린다.
		UAbilityTask_WaitDelay* FadeTask = UAbilityTask_WaitDelay::WaitDelay(this, EndFadeOutTime);
		FadeTask->OnFinish.AddDynamic(this, &UGABossExecution::OnEndFadeFinished);
		FadeTask->ReadyForActivation();
		return;
	}

	OnEndFadeFinished();
}

void UGABossExecution::OnEndFadeFinished()
{
	bReturnBossOnEnd = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGABossExecution::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	// 중간에 끊겨도 게임이 느린 채로 남지 않게 먼저 속도를 되돌린다.
	StopImpactTimeDilation(true);

	SetIgnoreVictimCollision(false);

	// 보스가 살아 있으면 AI를 다시 켠다.
	ABossEnemy* Boss = Cast<ABossEnemy>(GetAvatarActorFromActorInfo());

	// 끝까지 연출했으면 화면이 어두운 동안 보스를 처형 직전 자리로 되돌린다. 플레이어 높이(공중)에 남아 떨어지지 않게 한다.
	if (bReturnBossOnEnd && IsValid(Boss) && !Boss->IsDead())
	{
		Boss->GetCharacterMovement()->StopMovementImmediately();
		if (!Boss->TeleportTo(BossStartTransform.GetLocation(), BossStartTransform.Rotator()))
		{
			Boss->SetActorLocationAndRotation(BossStartTransform.GetLocation(), BossStartTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	bReturnBossOnEnd = false;

	// 납도를 마친 자세로 멈춰 둔 처형 몽타주를 끝낸다
	if (IsValid(Boss) && IsValid(ExecutionAttackMontage))
	{
		if (UAnimInstance* BossAnimInstance = Boss->GetMesh()->GetAnimInstance())
		{
			if (BossAnimInstance->Montage_IsPlaying(ExecutionAttackMontage))
			{
				BossAnimInstance->Montage_Stop(0.2f, ExecutionAttackMontage);
			}
		}
	}

	// 껐던 중력을 되돌린다. 공중이었다면 이제 떨어진다.
	if (bBossGravityDisabled && IsValid(Boss) && !Boss->IsDead())
	{
		UCharacterMovementComponent* Movement = Boss->GetCharacterMovement();
		if (Movement->MovementMode == MOVE_Flying)
		{
			Movement->SetMovementMode(SavedBossMovementMode == MOVE_Walking || SavedBossMovementMode == MOVE_NavWalking
				? MOVE_Falling : SavedBossMovementMode.GetValue());
		}
	}
	bBossGravityDisabled = false;

	if (PausedBrain.IsValid() && IsValid(Boss) && !Boss->IsDead())
	{
		PausedBrain->ResumeLogic(TEXT("BossExecution finished"));
	}

	Victim.Reset();
	PausedBrain.Reset();
	ExecutionAttackMontage = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
