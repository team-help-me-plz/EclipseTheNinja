#include "GAS/Ability/GAAssassinateBase.h"
#include "GAS/Ability/GABelowAssassinate.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GAS/Task/AbilityTask_MontageRootOffset.h"
#include "GAS/Task/AbilityTask_SlideActor.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GAS/Animation/AnimNotify_AssassinateImpact.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Curves/CurveVector.h"
#include "GameFramework/RootMotionSource.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BrainComponent.h"
#include "Character/BossEnemy.h"
#include "Character/Enemy.h"
#include "Character/PlayerCharacter.h"
#include "Component/AssassinateTargetComponent.h"
#include "Component/AssassinationDropComponent.h"
#include "Component/AssassinationTargetingComponent.h"
#include "Component/EnemyDetectionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Ability/GAWallAttach.h"
#include "GAS/StatAttributeSet.h"
#include "NativeGameplayTags.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Assassinating, "State.Assassinating");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AssassinateImpact, "Event.Assassinate.Impact");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AssassinateVFX, "Event.Assassinate.VFX");

UGAAssassinateBase::UGAAssassinateBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	// 피격 이펙트는 암살 어빌리티 Blueprint의 기본값에서 지정한다.

	// 암살 중일 때는 태그를 얻어서 중복 실행 방지
	ActivationOwnedTags.AddTag(TAG_Assassinating);
	ActivationOwnedTags.AddTag(UGABaseAttack::GetAttackingTag());
	ActivationBlockedTags.AddTag(TAG_Assassinating);
	ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());
	// 벽에 매달린 상태에서 시작하면 매달리기를 먼저 끝낸다 (ActivateAbility 전에 취소된다)
	CancelAbilitiesWithTag.AddTag(UGAWallAttach::GetWallAttachAbilityTag());
}

void UGAAssassinateBase::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	if (HasAnyFlags(RF_ClassDefaultObject)) return;

	// 시전할 때 쓰는 에셋을 비동기로 불러 둔다. 다 불러오기 전에 시전하면 기존처럼 그 자리에서 불러온다.
	TArray<FSoftObjectPath> Paths = {
		AttackMontage.ToSoftObjectPath(), VictimMontage.ToSoftObjectPath(),
		bUseImpactVFX ? ImpactVFX.ToSoftObjectPath() : FSoftObjectPath() };
	AddPreloadPaths(Paths);
	AssetPreloader.Request(Paths);
}

void UGAAssassinateBase::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	AssetPreloader.Cancel();
	Super::OnRemoveAbility(ActorInfo, Spec);
}

FGameplayTag UGAAssassinateBase::GetAssassinatingTag()
{
	return TAG_Assassinating;
}

FGameplayTag UGAAssassinateBase::GetImpactEventTag()
{
	return TAG_AssassinateImpact;
}

FGameplayTag UGAAssassinateBase::GetVFXEventTag()
{
	return TAG_AssassinateVFX;
}

bool UGAAssassinateBase::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	if (!ActorInfo) return false;

	const APlayerCharacter* Player = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Player)) return false;

	const UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(SourceASC)) return false;
	if (SourceASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f) return false;

	// 플레이어가 암살 가능한 캐릭터가 아니면 종료
	const UAssassinationTargetingComponent* Targeting = Player->GetAssassinationTargetingComponent();
	if (!IsValid(Targeting)) return false;

	// 현재 암살 타겟의 암살 가능 구역에 플레이어가 있는지 재확인
	// 타겟이 암살 가능한 상태인지 재 확인
	UAssassinateTargetComponent* Target = Targeting->GetCurrentTarget();
	if (!IsValid(Target)) return false;
	if (!Target->IsPlayerInZone()) return false;
	if (!Target->CanAssassinate(Player)) return false;

	// 타겟이 공중에 있는 경우는 실행불가
	const AEnemy* Enemy = Cast<AEnemy>(Target->GetOwner());
	if (!IsValid(Enemy)) return false;
	if (!Enemy->GetCharacterMovement()->IsMovingOnGround()) return false;

	return CanAssassinateTarget(Player, Target, Enemy);
}

void UGAAssassinateBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	bEnding = false;
	bMovementLocked = false;
	bTargetReserved = false;
	bAttackRootMotionDisabled = false;
	bIgnoringVictimCollision = false;
	bLedgeWalkLocked = false;
	bImpactTriggered = false;
	bPlayBossImpactSound = false;
	bVictimExecuted = false;
	bImpactVFXTriggered = false;
	PausedBrain.Reset();

	// 암살자 설정
	Assassin = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());

	// 암살자나 암살대상이 없으면 종료
	APlayerCharacter* Player = Assassin.Get();
	if (!IsValid(Player)) { OnInterrupted(); return; }

	UAssassinationTargetingComponent* Targeting = Player->GetAssassinationTargetingComponent();
	if (!IsValid(Targeting)) { OnInterrupted(); return; }

	// 현재 암살 타겟이 유효하지 않거나 암살 불가능한 상태면 종료
	TargetComponent = Targeting->GetCurrentTarget();
	if (!TargetComponent.IsValid()) { OnInterrupted(); return; }
	if (!TargetComponent->CanAssassinate(Player)) { OnInterrupted(); return; }

	// 암살 대상
	Victim = Cast<AEnemy>(TargetComponent->GetOwner());
	AEnemy* Enemy = Victim.Get();
	if (!IsValid(Enemy)) { OnInterrupted(); return; }

	// 암살 모션과, 암살 피격 모션 동적으로 로딩
	ExecutionMontage = ResolveAttackMontage(Enemy).LoadSynchronous();
	VictimExecutionMontage = VictimMontage.LoadSynchronous();

	// 임팩트 프레임에서 에셋을 읽느라 연출이 멈추지 않도록 미리 준비한다.
	LoadedImpactVFX = nullptr;
	if (bUseImpactVFX)
	{
		LoadedImpactVFX = ImpactVFX.LoadSynchronous();
	}
	
	if (!IsValid(ExecutionMontage) || !IsValid(VictimExecutionMontage))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 암살 몽타주가 지정되지 않았습니다."), *GetNameSafe(GetClass()));
		OnInterrupted();
		return;
	}
	if (!Player->GetMesh()->GetAnimInstance()) { OnInterrupted(); return; }
	if (!Enemy->GetMesh()->GetAnimInstance()) { OnInterrupted(); return; }
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) { OnInterrupted(); return; }

	// 실행 중에는 타겟팅 결과가 바뀌어도 처음 확정한 적을 유지한다.
	TargetComponent->SetAssassinationInProgress(true);
	bTargetReserved = true;
	TargetComponent->SetTargetHighlighted(false);

	// 이미 실행 중인 공격 태스크도 멈춘다. 예약 상태를 먼저 기록해 재진입을 막는다.
	Enemy->CancelAIBaseAttack();

	// 칼이 꽂히는 위치와 회복 오브 드롭 위치가 어긋나지 않게 메시를 캡슐 위치로 돌린다.
	Enemy->SetMeshYOffsetSuppressed(true);

	// 하단·상단 암살은 피격자 캡슐을 옆으로 옮기므로, 옮기기 전 캡슐 Y(이동 평면)에 회복 오브를 떨어뜨리게 한다.
	if (UAssassinationDropComponent* DropComp = Enemy->FindComponentByClass<UAssassinationDropComponent>())
	{
		DropComp->SetDropPlaneY(Enemy->GetActorLocation().Y);
	}

	// 타겟이 파괴될때 실행할 함수를 바인딩
	Enemy->OnDestroyed.AddDynamic(this, &UGAAssassinateBase::OnVictimDestroyed);

	// 적의 움직임, BT와 관련한 처리를 해준다.
	if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
	{
		AI->StopMovement();
		UBrainComponent* Brain = AI->GetBrainComponent();
		if (IsValid(Brain) && !Brain->IsPaused())
		{
			Brain->PauseLogic(TEXT("Assassination"));
			PausedBrain = Brain;
		}
	}

	// 암살자의 움직임 멈춤
	UCharacterMovementComponent* PlayerMovement = Player->GetCharacterMovement();

	// 앉은 상태에서 시작하면 일어서서 암살한다. 접근 위치가 서 있는 높이로 계산되도록 바로 일어선다.
	if (Player->bIsCrouched)
	{
		Player->UnCrouch();
		PlayerMovement->UnCrouch(false);
	}
	PlayerMovementMode = PlayerMovement->MovementMode;
	PlayerCustomMode = PlayerMovement->CustomMovementMode;
	PlayerMovement->StopMovementImmediately();
	Player->StopJumping();

	// 공중에서 시작하는 경우는 벽에 매달려 있던 경우뿐이다 (매달리기가 취소되며 Falling이 된다).
	// 준비 동작 동안 떨어지지 않게 그 자리에 고정하고, 끝나면 낙하 상태로 돌려 바닥을 다시 찾게 한다.
	if (PlayerMovement->IsFalling())
	{
		PlayerMovementMode = MOVE_Falling;
		PlayerCustomMode = 0;
		PlayerMovement->SetMovementMode(MOVE_Flying);
	}

	// 둘 다 움직임 멈춤
	UCharacterMovementComponent* EnemyMovement = Enemy->GetCharacterMovement();
	VictimMovementMode = EnemyMovement->MovementMode;
	VictimCustomMode = EnemyMovement->CustomMovementMode;
	EnemyMovement->StopMovementImmediately();
	EnemyMovement->DisableMovement();
	bMovementLocked = true;

	// 피격자 몽타주가 등 뒤 기준이므로 적과 같은 방향을 바라본다.ㄴ
	ExecutionRotation = Enemy->GetActorRotation();
	Player->SetAssassinationFacing(ExecutionRotation.Yaw);

	// 기본·상단 암살은 칼을 뽑는 프레임의 VFX 노티파이에서 사운드도 재생한다.
	if (bUseImpactVFX || !IsBareHandedExecution())
	{
		UAbilityTask_WaitGameplayEvent* VFXTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, TAG_AssassinateVFX, nullptr, true, true);
		VFXTask->EventReceived.AddDynamic(this, &UGAAssassinateBase::OnVFXEvent);
		VFXTask->ReadyForActivation();
	}

	StartApproach();
}

void UGAAssassinateBase::PlayExecution()
{
	// 연출이 시작되는 시점에 기존 사망 처리로 처치를 확정한다.
	if (!PlayAttackMontage()) return;
	ExecuteVictim();
}

bool UGAAssassinateBase::PlayAttackMontage()
{
	if (bEnding || !IsActive()) return false;

	APlayerCharacter* Player = Assassin.Get();
	if (!IsValid(Player)) { OnInterrupted(); return false; }

	AEnemy* Enemy = Victim.Get();
	if (!IsValid(Enemy) || Enemy->IsDead()) { OnInterrupted(); return false; }

	// 암살자가 암살대상자와 같은 방향을 바라보도록
	Player->SetAssassinationFacing(ExecutionRotation.Yaw);

	return StartAttackMontageTask(0.0f);
}

void UGAAssassinateBase::SlideVictim(float SideOffset, float ForwardOffset, float TimeToContact)
{
	if (FMath::IsNearlyZero(SideOffset) && FMath::IsNearlyZero(ForwardOffset)) return;

	// 피격자가 가만히 서 있는 동안 미끄러지면 눈에 띄므로, 맞물리기 직전 VictimSlideTime 동안만 옮긴다.
	PendingVictimSideOffset = SideOffset;
	PendingVictimForwardOffset = ForwardOffset;
	PendingVictimSlideDuration = FMath::Min(VictimSlideTime, TimeToContact);
	const float Delay = TimeToContact - PendingVictimSlideDuration;
	if (Delay <= KINDA_SMALL_NUMBER)
	{
		StartVictimSlide();
		return;
	}

	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, Delay);
	WaitTask->OnFinish.AddDynamic(this, &UGAAssassinateBase::StartVictimSlide);
	WaitTask->ReadyForActivation();
}

void UGAAssassinateBase::StartVictimSlide()
{
	if (bEnding || !IsActive()) return;

	AEnemy* Enemy = Victim.Get();
	if (!IsValid(Enemy)) return;

	// 피격자는 암살자와 같은 방향을 보므로 피격자의 오른쪽·앞이 곧 암살자의 오른쪽·앞이다.
	// 옆은 화면 깊이 방향(월드 Y축), 앞은 피격자가 바라보는 방향이다.
	VictimSlideDelta = Enemy->GetActorRightVector().GetSafeNormal2D() * PendingVictimSideOffset
		+ Enemy->GetActorForwardVector().GetSafeNormal2D() * PendingVictimForwardOffset;
	VictimSlideOrigin = Enemy->GetActorLocation();
	bVictimSlidePending = true;

	UAbilityTask_SlideActor* SlideTask = UAbilityTask_SlideActor::SlideActor(
		this, Enemy, Enemy->GetActorLocation() + VictimSlideDelta, PendingVictimSlideDuration);
	SlideTask->ReadyForActivation();

	// 살아남는 피격자(보스)는 암살자 몽타주가 끝나기 직전에 원래 평면으로 되돌린다.
	if (Enemy->IsDead()) return;
	const APlayerCharacter* Player = Assassin.Get();
	const UAnimInstance* AnimInstance = IsValid(Player) ? Player->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !IsValid(ExecutionMontage)) return;

	const float Remaining = ExecutionMontage->GetPlayLength() - AnimInstance->Montage_GetPosition(ExecutionMontage);
	const float ReturnDelay = FMath::Max(PendingVictimSlideDuration, Remaining - VictimSlideTime - 0.05f);
	UAbilityTask_WaitDelay* ReturnTask = UAbilityTask_WaitDelay::WaitDelay(this, ReturnDelay);
	ReturnTask->OnFinish.AddDynamic(this, &UGAAssassinateBase::ReturnSlidVictim);
	ReturnTask->ReadyForActivation();
}

void UGAAssassinateBase::ReturnSlidVictim()
{
	if (!bVictimSlidePending) return;

	AEnemy* Enemy = Victim.Get();
	if (!IsValid(Enemy) || Enemy->IsDead()) return;

	// 옮긴 방향으로 남아 있는 거리만 되돌린다. 되돌리는 도중에 끊겨도 남은 만큼만 옮긴다.
	const FVector SlideDirection = VictimSlideDelta.GetSafeNormal();
	const float RemainingDistance = FVector::DotProduct(Enemy->GetActorLocation() - VictimSlideOrigin, SlideDirection);
	const FVector Target = Enemy->GetActorLocation() - SlideDirection * RemainingDistance;
	if (bEnding || !IsActive())
	{
		// 암살이 먼저 끝났으면 태스크를 쓸 수 없으므로 바로 옮긴다.
		Enemy->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
		bVictimSlidePending = false;
		return;
	}
	UAbilityTask_SlideActor* SlideTask = UAbilityTask_SlideActor::SlideActor(this, Enemy, Target, VictimSlideTime);
	SlideTask->ReadyForActivation();
}

bool UGAAssassinateBase::RestartAttackMontageAt(float StartTime)
{
	if (bEnding || !IsActive()) return false;

	// 같은 몽타주를 새로 재생하면 이전 재생은 섞여 빠지면서 중단 알림을 보낸다. 그 알림으로 어빌리티가 끝나지 않게 한다.
	// 새 재생은 루트 모션이 켜진 상태로 시작한다.
	bRestartingAttackMontage = true;
	bAttackRootMotionDisabled = false;
	const bool bStarted = StartAttackMontageTask(StartTime);
	bRestartingAttackMontage = false;
	return bStarted;
}

bool UGAAssassinateBase::StartAttackMontageTask(float StartTime)
{
	// MonTage Task 실행
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, ExecutionMontage, 1.0f, NAME_None, true, 1.0f, StartTime);
	MontageTask->OnCompleted.AddDynamic(this, &UGAAssassinateBase::OnCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGAAssassinateBase::OnInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UGAAssassinateBase::OnInterrupted);
	MontageTask->ReadyForActivation();
	return IsActive();
}

void UGAAssassinateBase::ExecuteVictim()
{
	if (bEnding || !IsActive()) return;

	AEnemy* Enemy = Victim.Get();
	if (!IsValid(Enemy) || Enemy->IsDead()) { OnInterrupted(); return; }

	// 암살 피격 애니메이션 수신
	ABossEnemy* Boss = Cast<ABossEnemy>(Enemy);
	const float PreviousBossHealthRatio = Boss ? Boss->GetHealthRatio() : 0.0f;
	if (Boss)
	{
		Boss->SetIncomingAssassinationType(GetAssassinationType());
	}
	Enemy->ReceiveAssassination(VictimExecutionMontage);
	bVictimExecuted = true;
	bPlayBossImpactSound = IsValid(Boss) && Boss->GetHealthRatio() < PreviousBossHealthRatio;

	// 실제 재생 중인 몽타주를 불러옴
	UAnimInstance* VictimAnimInstance = Enemy->GetMesh()->GetAnimInstance();
	UAnimMontage* PlayedMontage = VictimAnimInstance ? VictimAnimInstance->GetCurrentActiveMontage() : nullptr;
	if (!PlayedMontage) return;

	// 피격자는 이동이 꺼져 있으므로 끌려가거나 쓰러지며 움직이는 루트 이동을 메시로 보여주고, 쓰러진 자리에 둔다.
	UAbilityTask_MontageRootOffset* OffsetTask = UAbilityTask_MontageRootOffset::FollowMontageRoot(
		this, Enemy, PlayedMontage, false, true);
	OffsetTask->ReadyForActivation();
}

void UGAAssassinateBase::SetAttackRootMotionEnabled(bool bEnabled)
{
	if (bAttackRootMotionDisabled == !bEnabled) return;

	const APlayerCharacter* Player = Assassin.Get();
	UAnimInstance* AnimInstance = IsValid(Player) ? Player->GetMesh()->GetAnimInstance() : nullptr;
	FAnimMontageInstance* MontageInstance = AnimInstance ? AnimInstance->GetActiveInstanceForMontage(ExecutionMontage) : nullptr;
	if (!MontageInstance) return;

	if (bEnabled) MontageInstance->PopDisableRootMotion();
	else MontageInstance->PushDisableRootMotion();
	bAttackRootMotionDisabled = !bEnabled;
}

void UGAAssassinateBase::HandOverToAttackRootMotion()
{
	APlayerCharacter* Player = Assassin.Get();
	if (!IsValid(Player)) return;

	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Walking);
	if (!bLedgeWalkLocked)
	{
		bSavedCanWalkOffLedges = Movement->bCanWalkOffLedges;
		Movement->bCanWalkOffLedges = false;
		bLedgeWalkLocked = true;
	}
	SetAttackRootMotionEnabled(true);
}

void UGAAssassinateBase::SetIgnoreVictimCollision(bool bIgnore)
{
	if (bIgnoringVictimCollision == bIgnore) return;

	APlayerCharacter* Player = Assassin.Get();
	AEnemy* Enemy = Victim.Get();
	if (IsValid(Player) && IsValid(Enemy))
	{
		Player->GetCapsuleComponent()->IgnoreActorWhenMoving(Enemy, bIgnore);
	}
	bIgnoringVictimCollision = bIgnore;
}

UAbilityTask_ApplyRootMotionMoveToForce* UGAAssassinateBase::MoveAlongAttackRootMotion(
	float StartTime, float Duration, const FVector& End)
{
	const APlayerCharacter* Player = Assassin.Get();
	if (!IsValid(Player) || !IsValid(ExecutionMontage)) return nullptr;

	if (!IsValid(RootMotionPathCurve))
	{
		RootMotionPathCurve = NewObject<UCurveVector>(this);
	}

	// 이동 태스크는 시작~끝을 일정한 속도로 잇고 이 곡선을 더한다(X: 진행 방향, Z: 위).
	// 몽타주 루트 모션이 해당 구간에서 진행한 비율을 따라가도록 직선과의 차이를 넣어, 실제 거리·높이가 달라도
	// 애니메이션과 같은 가감속 궤적이 되게 한다.
	FRichCurve& ForwardCurve = RootMotionPathCurve->FloatCurves[0]; // 앞뒤
	FRichCurve& UpCurve = RootMotionPathCurve->FloatCurves[2];      // 위아래
	ForwardCurve.Reset();
	UpCurve.Reset();

	const FAnimExtractContext Context;
	// 애니메이션이 이 구간에서 움직인 전체 이동량을 수평과 수직으로 나눈다.
	const FVector AnimTotal = ExecutionMontage->ExtractRootMotionFromTrackRange(
		StartTime, StartTime + Duration, Context).GetTranslation();
	const FVector AnimDirection = AnimTotal.GetSafeNormal2D();
	const float AnimHorizontal = AnimTotal.Size2D();
	const float AnimVertical = AnimTotal.Z;

	// 게임에서 실제로 이동해야 할 거리
	const FVector WorldDelta = End - Player->GetActorLocation();
	const float WorldHorizontal = WorldDelta.Size2D();
	const float WorldVertical = WorldDelta.Z;

	constexpr int32 SampleCount = 16;
	for (int32 Index = 0; Index <= SampleCount; ++Index)
	{
		const float Fraction = static_cast<float>(Index) / SampleCount;
		const FVector AnimPartial = ExecutionMontage->ExtractRootMotionFromTrackRange(
			StartTime, StartTime + Duration * Fraction, Context).GetTranslation();

		// 진행률 = 현재까지 이동량 / 전체 이동량. 수평은 진행 방향으로 투영해 뒤로 물러나는 움직임도 살린다.
		// 애니메이션에 해당 축 이동이 거의 없으면 직선 진행으로 둔다.
		const float HorizontalProgress = AnimHorizontal > 1.0f
			? FVector::DotProduct(FVector(AnimPartial.X, AnimPartial.Y, 0.0f), AnimDirection) / AnimHorizontal : Fraction;
		const float VerticalProgress = FMath::Abs(AnimVertical) > 1.0f ? AnimPartial.Z / AnimVertical : Fraction;

		// 키 사이를 직선으로 이으면 키마다 속도가 계단처럼 바뀌므로 부드러운 곡선으로 잇는다.
		ForwardCurve.SetKeyInterpMode(ForwardCurve.AddKey(Fraction, (HorizontalProgress - Fraction) * WorldHorizontal), RCIM_Cubic);
		UpCurve.SetKeyInterpMode(UpCurve.AddKey(Fraction, (VerticalProgress - Fraction) * WorldVertical), RCIM_Cubic);
	}
	ForwardCurve.AutoSetTangents();
	UpCurve.AutoSetTangents();

	UAbilityTask_ApplyRootMotionMoveToForce* MoveTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this, NAME_None, End, FMath::Max(0.05f, Duration), false, MOVE_Flying, false, RootMotionPathCurve,
		ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.0f);
	return MoveTask;
}

void UGAAssassinateBase::OnVFXEvent(FGameplayEventData Payload)
{
	if (!bEnding && IsActive() && !IsBareHandedExecution())
	{
		if (const APlayerCharacter* Player = Assassin.Get())
		{
			Player->PlayAssassinationWithdrawalSound();
		}
	}
	PlayImpactVFX();
}

void UGAAssassinateBase::WaitForImpact(float CurrentMontageTime)
{
	if (bEnding) return;
	if (!IsActive()) return;

	// 몽타주는 1배속으로 재생하므로, 지금 몽타주 위치부터 잰 게임 시간 대기가 몽타주 시간과 같다.
	// 예비 동작 슬로모가 걸려도 대기 시간이 같이 늘어나 시점이 어긋나지 않는다.
	float ImpactTime = ImpactFallbackTime;
	if (FindImpactNotifyTime(ImpactTime))
	{
		// 노티파이는 몽타주가 틱할 때 오므로 재생 전에 듣기 시작한다.
		UAbilityTask_WaitGameplayEvent* ImpactTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, TAG_AssassinateImpact, nullptr, true);
		ImpactTask->EventReceived.AddDynamic(this, &UGAAssassinateBase::OnImpactEvent);
		ImpactTask->ReadyForActivation();
	}
	else if (ImpactFallbackTime >= 0.0f)
	{
		// 노티파이를 아직 안 찍은 몽타주도 바로 확인할 수 있게 고정 시점으로 대신한다.
		UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(
			this, FMath::Max(0.0f, ImpactFallbackTime - CurrentMontageTime));
		DelayTask->OnFinish.AddDynamic(this, &UGAAssassinateBase::OnImpactFallback);
		DelayTask->ReadyForActivation();
	}
	else
	{
		return;
	}

	if (!IsBareHandedExecution())
	{
		constexpr float StabSoundLeadTime = 0.08f;
		UAbilityTask_WaitDelay* StabSoundTask = UAbilityTask_WaitDelay::WaitDelay(
			this, FMath::Max(0.0f, ImpactTime - CurrentMontageTime - StabSoundLeadTime));
		StabSoundTask->OnFinish.AddDynamic(this, &UGAAssassinateBase::PlayAssassinationStabSound);
		StabSoundTask->ReadyForActivation();
	}

	// 예비 동작: 칼을 뻗기 전까지 느리게 보여 긴장을 만들고, 칼이 들어가는 구간은 원래 속도로 되돌려 빠르게 꽂는다.
	if (!bUseImpactTimeDilation || !bUseAnticipation || AnticipationLeadTime <= SnapLeadTime) return;
	const float AnticipationStart = ImpactTime - AnticipationLeadTime - CurrentMontageTime;
	const float SnapTime = ImpactTime - SnapLeadTime - CurrentMontageTime;
	if (AnticipationStart <= KINDA_SMALL_NUMBER || SnapTime <= KINDA_SMALL_NUMBER) return;

	UAbilityTask_WaitDelay* AnticipationTask = UAbilityTask_WaitDelay::WaitDelay(this, AnticipationStart);
	AnticipationTask->OnFinish.AddDynamic(this, &UGAAssassinateBase::OnAnticipationStart);
	AnticipationTask->ReadyForActivation();

	UAbilityTask_WaitDelay* SnapTask = UAbilityTask_WaitDelay::WaitDelay(this, SnapTime);
	SnapTask->OnFinish.AddDynamic(this, &UGAAssassinateBase::OnAnticipationSnap);
	SnapTask->ReadyForActivation();
}

void UGAAssassinateBase::OnImpactEvent(FGameplayEventData Payload)
{
	TriggerImpact();
}

void UGAAssassinateBase::OnImpactFallback()
{
	TriggerImpact();
}

void UGAAssassinateBase::TriggerImpact()
{
	if (bImpactTriggered) return;
	if (bEnding) return;
	if (!IsActive()) return;
	bImpactTriggered = true;

	if (IsBareHandedExecution())
	{
		PlayBelowAssassinationSound();
	}

	if (bPlayBossImpactSound)
	{
		if (ABossEnemy* Boss = Cast<ABossEnemy>(Victim.Get()))
		{
			Boss->PlayAssassinationImpactSound();
		}
	}

	if (!bUseImpactTimeDilation) return;
	StartImpactTimeDilation();
}

void UGAAssassinateBase::PlayAssassinationStabSound()
{
	if (!bEnding && IsActive())
	{
		if (const APlayerCharacter* Player = Assassin.Get())
		{
			Player->PlayActionSound(EPlayerActionSound::AssassinationStab);
		}
	}
}

void UGAAssassinateBase::PlayBelowAssassinationSound()
{
	if (BelowAssassinationSoundDelay <= 0.0f)
	{
		if (const APlayerCharacter* Player = Assassin.Get())
		{
			Player->PlayActionSound(EPlayerActionSound::BelowAssassination);
		}

		return;
	}

	// 임팩트 직후 히트스톱으로 게임 시간이 거의 멈추므로 실제 시간으로 잰다
	const TWeakObjectPtr<APlayerCharacter> WeakPlayer = Assassin;
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[WeakPlayer](float DeltaTime)
			{
				if (const APlayerCharacter* Player = WeakPlayer.Get())
				{
					Player->PlayActionSound(EPlayerActionSound::BelowAssassination);
				}

				return false;
			}
		),
		BelowAssassinationSoundDelay
	);
}

void UGAAssassinateBase::PlayImpactVFX()
{
	if (bEnding) return;
	if (!IsActive()) return;
	if (bImpactVFXTriggered) return;
	if (!bUseImpactVFX) return;
	if (!IsValid(LoadedImpactVFX)) return;

	const AEnemy* Enemy = Victim.Get();
	if (!IsValid(Enemy)) return;

	const USkeletalMeshComponent* Mesh = Enemy->GetMesh();
	if (!IsValid(Mesh)) return;

	FTransform Origin = Enemy->GetActorTransform();
	if (Mesh->DoesSocketExist(ImpactVFXSocket))
	{
		Origin = Mesh->GetSocketTransform(ImpactVFXSocket);
	}
	const FTransform SpawnTransform = ImpactVFXOffset * Origin;

	// 파라미터를 모두 설정한 뒤 활성화해야 첫 프레임부터 지정한 크기·수명으로 생성된다.
	UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this, LoadedImpactVFX, SpawnTransform.GetLocation(), SpawnTransform.Rotator(),
		SpawnTransform.GetScale3D(), true, false, ENCPoolMethod::AutoRelease);
	if (!IsValid(Effect)) return;
	bImpactVFXTriggered = true;

	Effect->SetVariableFloat(TEXT("User.LifeTimeMult"), ImpactVFXLifeTime);
	Effect->SetVariableFloat(TEXT("User.Scale"), ImpactVFXScale);
	Effect->SetVariableFloat(TEXT("User.TextureScale"), ImpactVFXTextureScale);
	Effect->Activate(true);
}

void UGAAssassinateBase::OnAnticipationStart()
{
	if (bImpactTriggered || bEnding || !IsActive()) return;

	// 한 번에 떨어뜨리면 끊겨 보이므로 티커에서 서서히 느려지게 한다.
	SetTimeScale(1.0f);
	TimeDilationPhase = ETimeDilationPhase::Anticipation;
	ImpactElapsed = 0.0f;
	StartTimeDilationTicker();
}

void UGAAssassinateBase::OnAnticipationSnap()
{
	if (bImpactTriggered || bEnding || !IsActive()) return;

	// 칼이 들어가는 구간은 즉시 원래 속도로 돌려 빠르게 꽂히는 대비를 만든다.
	StopTimeDilationTicker();
	TimeDilationPhase = ETimeDilationPhase::None;
	SetTimeScale(1.0f);
}

bool UGAAssassinateBase::FindImpactNotifyTime(float& OutTime) const
{
	if (!IsValid(ExecutionMontage)) return false;

	// 노티파이는 몽타주 자체에도, 몽타주가 담은 시퀀스에도 찍을 수 있다.
	for (const FAnimNotifyEvent& Event : ExecutionMontage->Notifies)
	{
		if (!Cast<UAnimNotify_AssassinateImpact>(Event.Notify)) continue;
		OutTime = Event.GetTriggerTime();
		return true;
	}

	for (const FSlotAnimationTrack& Slot : ExecutionMontage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
		{
			const UAnimSequenceBase* Animation = Segment.GetAnimReference();
			if (!IsValid(Animation)) continue;

			for (const FAnimNotifyEvent& Event : Animation->Notifies)
			{
				if (!Cast<UAnimNotify_AssassinateImpact>(Event.Notify)) continue;

				// 시퀀스 안의 시간을 몽타주 시간으로 바꾼다. 구간 밖에 찍힌 노티파이는 재생되지 않는다.
				const float AnimTime = Event.GetTriggerTime();
				if (AnimTime < Segment.AnimStartTime || AnimTime > Segment.AnimEndTime) continue;
				const float PlayRate = FMath::Max(FMath::Abs(Segment.GetValidPlayRate()), KINDA_SMALL_NUMBER);
				OutTime = Segment.StartPos + (AnimTime - Segment.AnimStartTime) / PlayRate;
				return true;
			}
		}
	}
	return false;
}

void UGAAssassinateBase::SetTimeScale(float Scale)
{
	// 분신술 조준처럼 이미 느려진 상태일 수 있으므로 처음 바꿀 때의 배율을 기준으로 곱하고 그대로 되돌린다.
	if (!bTimeDilationApplied)
	{
		SavedTimeDilation = UGameplayStatics::GetGlobalTimeDilation(this);
		bTimeDilationApplied = true;
	}
	UGameplayStatics::SetGlobalTimeDilation(this, SavedTimeDilation * Scale);
}

void UGAAssassinateBase::StartImpactTimeDilation()
{
	TimeDilationPhase = ETimeDilationPhase::Impact;
	ImpactElapsed = 0.0f;
	SetTimeScale(HitStopTimeScale);
	StartTimeDilationTicker();
}

void UGAAssassinateBase::StartTimeDilationTicker()
{
	if (ImpactTickerHandle.IsValid()) return;
	ImpactTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UGAAssassinateBase::TickImpactTimeDilation));
}

void UGAAssassinateBase::StopTimeDilationTicker()
{
	if (ImpactTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ImpactTickerHandle);
	}
	ImpactTickerHandle.Reset();
}

bool UGAAssassinateBase::TickImpactTimeDilation(float DeltaTime)
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
		const float Alpha = FMath::Clamp(ImpactElapsed / FMath::Max(AnticipationBlendTime, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
		UGameplayStatics::SetGlobalTimeDilation(this,
			SavedTimeDilation * FMath::InterpEaseOut(1.0f, AnticipationTimeScale, Alpha, 2.0f));
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

void UGAAssassinateBase::StopImpactTimeDilation(bool bRemoveTicker)
{
	if (bRemoveTicker)
	{
		StopTimeDilationTicker();
	}
	ImpactTickerHandle.Reset();
	TimeDilationPhase = ETimeDilationPhase::None;

	if (!bTimeDilationApplied) return;
	bTimeDilationApplied = false;
	if (GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(this, SavedTimeDilation);
	}
}

void UGAAssassinateBase::OnCompleted()
{
	if (bEnding) return;
	if (!IsActive()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGAAssassinateBase::OnInterrupted()
{
	if (bRestartingAttackMontage) return;
	if (bEnding) return;
	if (!IsActive()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

void UGAAssassinateBase::OnVictimDestroyed(AActor* DestroyedActor)
{
	OnInterrupted();
}

void UGAAssassinateBase::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 끝났다는 표시 남기기
	if (bEnding) return;

	// 중간에 끊겨도 게임이 느린 채로 남지 않게 먼저 속도를 되돌린다.
	StopImpactTimeDilation(true);

	// 대상 참조를 비우기 전에 충돌 무시, 발판 끝 이동, 루트 모션 설정을 되돌린다.
	SetIgnoreVictimCollision(false);
	if (bLedgeWalkLocked)
	{
		if (APlayerCharacter* LedgePlayer = Assassin.Get())
		{
			LedgePlayer->GetCharacterMovement()->bCanWalkOffLedges = bSavedCanWalkOffLedges;
		}
		bLedgeWalkLocked = false;
	}
	SetAttackRootMotionEnabled(true);
	bAttackRootMotionDisabled = false;
	bEnding = true;

	// 암살자가 유효하고 움직임이 잠긴 경우
	APlayerCharacter* Player = Assassin.Get();
	if (IsValid(Player) && bMovementLocked)
	{

		UAbilitySystemComponent* SourceASC = Player->GetAbilitySystemComponent();
		if (IsValid(SourceASC) && SourceASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) > 0.0f)
		{
			// 플레이어 움직임 해제
			Player->GetCharacterMovement()->SetMovementMode(PlayerMovementMode, PlayerCustomMode);
		}
	}

	// BT 재개가 즉시 실행되더라도 암살 예약이 남아 있지 않게 한다.
	if (bTargetReserved && TargetComponent.IsValid())
	{
		TargetComponent->SetAssassinationInProgress(false);
	}

	// 암살 대상이 유효한 경우
	AEnemy* Enemy = Victim.Get();
	if (IsValid(Enemy))
	{
		// 정상적으로 암살이 끝났음녀 바인딩 해제하고
		// 만약 안죽었으면 움직임, BT 처리
		Enemy->OnDestroyed.RemoveDynamic(this, &UGAAssassinateBase::OnVictimDestroyed);
		if (!Enemy->IsDead() && bMovementLocked)
		{
			Enemy->GetCharacterMovement()->SetMovementMode(VictimMovementMode, VictimCustomMode);

			// 살아남았으면 평소 오프셋으로 되돌린다. 죽었으면 리스폰 때 되돌린다.
			Enemy->SetMeshYOffsetSuppressed(false);

			// 중간에 끊겨 아직 원래 평면으로 돌아오지 않았으면 지금 되돌린다.
			ReturnSlidVictim();

			// 암살 피해 시점에는 IsReadyForAI()가 false라 감지 갱신이 건너뛰어진다.
			// 예약을 푼 지금 암살자를 공격자로 알려 BT 재개 전에 전투 상태로 만든다.
			UEnemyDetectionComponent* Detection = Enemy->GetEneymDetectionComponent();
			if (bVictimExecuted && IsValid(Player) && IsValid(Detection))
			{
				Detection->NotifyDamaged(Player);
			}

			if (PausedBrain.IsValid())
			{
				PausedBrain->ResumeLogic(TEXT("Assassination cancelled"));
			}
		}
	}

	// 각종 변수 초기화
	bVictimSlidePending = false;
	bMovementLocked = false;
	bTargetReserved = false;
	Assassin.Reset();
	Victim.Reset();
	TargetComponent.Reset();
	PausedBrain.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
