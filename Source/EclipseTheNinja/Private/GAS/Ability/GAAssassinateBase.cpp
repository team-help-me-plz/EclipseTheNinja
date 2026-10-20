#include "GAS/Ability/GAAssassinateBase.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GAS/Task/AbilityTask_MontageRootOffset.h"
#include "GAS/Task/AbilityTask_SlideActor.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Components/CapsuleComponent.h"
#include "Curves/CurveVector.h"
#include "GameFramework/RootMotionSource.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BrainComponent.h"
#include "Character/Enemy.h"
#include "Character/PlayerCharacter.h"
#include "Component/AssassinateTargetComponent.h"
#include "Component/AssassinationTargetingComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/StatAttributeSet.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Assassinating, "State.Assassinating");

UGAAssassinateBase::UGAAssassinateBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	// 암살 중일 때는 태그를 얻어서 중복 실행 방지
	ActivationOwnedTags.AddTag(TAG_Assassinating);
	ActivationOwnedTags.AddTag(UGABaseAttack::GetAttackingTag());
	ActivationBlockedTags.AddTag(TAG_Assassinating);
	ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());
}

FGameplayTag UGAAssassinateBase::GetAssassinatingTag()
{
	return TAG_Assassinating;
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
	if (!Target->CanAssassinate()) return false;

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
	if (!TargetComponent->CanAssassinate()) { OnInterrupted(); return; }

	// 암살 대상
	Victim = Cast<AEnemy>(TargetComponent->GetOwner());
	AEnemy* Enemy = Victim.Get();
	if (!IsValid(Enemy)) { OnInterrupted(); return; }

	// 암살 모션과, 암살 피격 모션 동적으로 로딩
	ExecutionMontage = AttackMontage.LoadSynchronous();
	VictimExecutionMontage = VictimMontage.LoadSynchronous();
	
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
	PlayerMovementMode = PlayerMovement->MovementMode;
	PlayerCustomMode = PlayerMovement->CustomMovementMode;
	PlayerMovement->StopMovementImmediately();
	Player->StopJumping();

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
	const FVector Target = Enemy->GetActorLocation()
		+ Enemy->GetActorRightVector().GetSafeNormal2D() * PendingVictimSideOffset
		+ Enemy->GetActorForwardVector().GetSafeNormal2D() * PendingVictimForwardOffset;

	UAbilityTask_SlideActor* SlideTask = UAbilityTask_SlideActor::SlideActor(this, Enemy, Target, PendingVictimSlideDuration);
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
	Enemy->ReceiveAssassination(VictimExecutionMontage);

	// 피격자는 이동이 꺼져 있으므로 끌려가거나 쓰러지며 움직이는 루트 이동을 메시로 보여주고, 쓰러진 자리에 둔다.
	UAbilityTask_MontageRootOffset* OffsetTask = UAbilityTask_MontageRootOffset::FollowMontageRoot(
		this, Enemy, VictimExecutionMontage, false, true);
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
			if (PausedBrain.IsValid())
			{
				PausedBrain->ResumeLogic(TEXT("Assassination cancelled"));
			}
		}
	}

	// 타겟의 암살이 끝났다고 표시
	if (bTargetReserved && TargetComponent.IsValid())
	{
		TargetComponent->SetAssassinationInProgress(false);
	}

	// 각종 변수 초기화
	bMovementLocked = false;
	bTargetReserved = false;
	Assassin.Reset();
	Victim.Reset();
	TargetComponent.Reset();
	PausedBrain.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
