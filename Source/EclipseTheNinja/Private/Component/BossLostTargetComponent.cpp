#include "Component/BossLostTargetComponent.h"

#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/BossEnemy.h"
#include "Character/BossPatternRow.h"
#include "Component/EnemyDetectionComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace BossLostTarget
{
	// 이 거리(cm) 안이면 목표 지점(마지막 목격 위치, 난사 지점)에 도착한 것으로 본다.
	constexpr float ArrivalDistance = 30.0f;

	// 기대 이동량의 이 비율보다 적게 움직이면 막힌 것으로 본다.
	constexpr float BlockedMoveRatio = 0.1f;

	// 이 시간(초) 동안 막혀 있으면 이동을 포기하고 지금 위치에서 다음 행동을 한다. 보스의 제자리 회전 시간보다 길어야 한다.
	constexpr float BlockedGiveUpTime = 1.0f;

	// 쓸 수 있는 패턴이 없거나 활성화에 실패했을 때 다시 시도하기까지의 시간(초)
	constexpr float RetryDelay = 0.5f;

	// 마지막 목격 위치가 찾아본 위치에서 이 거리(cm) 넘게 바뀌면 새 단서로 본다.
	constexpr float NewClueDistance = 10.0f;
}

UBossLostTargetComponent::UBossLostTargetComponent()
{
	// BT 태스크가 구동하므로 스스로 Tick하지 않는다.
	PrimaryComponentTick.bCanEverTick = false;
}

void UBossLostTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	Boss = Cast<ABossEnemy>(GetOwner());
}

bool UBossLostTargetComponent::BeginLostTarget()
{
	ABossEnemy* Owner = Boss.Get();
	UEnemyDetectionComponent* Detection = Owner ? Owner->GetEneymDetectionComponent() : nullptr;
	if (!Owner || !Detection || Owner->IsDead())
	{
		return false;
	}

	// 쫓던 분신이 회수되거나 사라져 대상이 비었으면 플레이어를 찾는 것으로 본다.
	// 플레이어까지 죽은 경우(플레이어 처치 직후)는 놓친 것이 아니므로 시작하지 않는다.
	APawn* Target = Detection->GetCurrentTarget();
	if (!Owner->IsValidAITarget(Target))
	{
		Target = UGameplayStatics::GetPlayerPawn(this, 0);
	}

	if (!Owner->IsValidAITarget(Target))
	{
		return false;
	}

	// 먼저 마지막 목격 위치까지 가 보고, 거기서도 못 찾으면 포효한다.
	LostTarget = Target;
	StartApproach();
	return true;
}

void UBossLostTargetComponent::StartApproach()
{
	const ABossEnemy* Owner = Boss.Get();

	LostTargetState = EBossLostTargetState::Approaching;
	PreviousX = Owner->GetActorLocation().X;
	BlockedTime = 0.0f;

	UE_LOG(LogTemp, Log, TEXT("%s: 마지막 목격 위치로 이동"), *Owner->GetName());
}

bool UBossLostTargetComponent::HasNewClue(const UEnemyDetectionComponent& Detection) const
{
	// 포효·난사 중에 마지막 목격 위치가 바뀌는 경우는 피격으로 공격자 위치를 새로 알게 됐을 때뿐이다.
	// (보이면 놓침 처리가 끝나고, 전투 중에는 소리로 위치를 갱신하지 않는다)
	return Detection.HasLastKnownLocation()
		&& FVector::DistSquared(Detection.GetLastKnownLocation(), SearchedLocation) > FMath::Square(BossLostTarget::NewClueDistance);
}

bool UBossLostTargetComponent::UpdateLostTarget(float DeltaTime)
{
	ABossEnemy* Owner = Boss.Get();
	UEnemyDetectionComponent* Detection = Owner ? Owner->GetEneymDetectionComponent() : nullptr;
	if (!Owner || !Detection || Owner->IsDead())
	{
		EndLostTarget(false);
		return false;
	}

	const bool bInCombat = Detection->GetCurrentAlertLevel() == EEnemyAlertLevel::Combat;
	const bool bHasSight = Detection->bHasLineOfSight;

	switch (LostTargetState)
	{
	case EBossLostTargetState::Approaching:
		// 가는 도중 대상을 다시 보면 포효 없이 일반 전투로 돌아간다.
		if (!bInCombat || bHasSight)
		{
			EndLostTarget(false);
			return false;
		}

		// 도착했거나 길이 막혀 더 갈 수 없으면 그 자리에서 포효한다.
		if (!Detection->HasLastKnownLocation()
			|| !MoveBossTowardX(Detection->GetLastKnownLocation().X, DeltaTime))
		{
			StartRoar();
		}
		return true;

	case EBossLostTargetState::Roaring:
		// 포효는 대상을 다시 봐도 끝까지 재생한다. 끝난 뒤에도 놓친 상태면 난사로 넘어간다.
		if (IsRoaring())
		{
			return true;
		}

		if (!bInCombat || bHasSight)
		{
			EndLostTarget(false);
			return false;
		}

		// 포효하는 동안 공격받아 새 위치를 알게 됐으면 난사 대신 그 위치로 간다.
		if (HasNewClue(*Detection))
		{
			StartApproach();
			return true;
		}

		StartRampage();
		return true;

	case EBossLostTargetState::Rampaging:
		// 대상을 다시 보면 일반 전투로 돌아간다. 시전 중인 패턴은 그대로 끝까지 재생한다.
		if (!bInCombat || bHasSight)
		{
			EndLostTarget(false);
			return false;
		}

		// 난사 중 공격받아 새 위치를 알게 되면, 시전 중인 패턴을 마친 뒤 난사를 멈추고 그 위치로 달려간다.
		if (HasNewClue(*Detection) && !Owner->IsAttackActive())
		{
			StartApproach();
			return true;
		}

		UpdateRampage(DeltaTime);
		return IsHandlingLostTarget();

	default:
		return false;
	}
}

void UBossLostTargetComponent::StopLostTarget()
{
	EndLostTarget(false);
}

void UBossLostTargetComponent::InterruptLostTarget()
{
	StopLostTarget();

	if (UAudioComponent* Audio = RoarAudio.Get())
	{
		Audio->Stop();
	}
	RoarAudio.Reset();

	const ABossEnemy* Owner = Boss.Get();
	UAnimInstance* AnimInstance = Owner ? Owner->GetMesh()->GetAnimInstance() : nullptr;
	if (RoarMontage && AnimInstance)
	{
		AnimInstance->Montage_Stop(0.0f, RoarMontage);
	}
}

bool UBossLostTargetComponent::IsRoaring() const
{
	const ABossEnemy* Owner = Boss.Get();
	const UAnimInstance* AnimInstance = Owner ? Owner->GetMesh()->GetAnimInstance() : nullptr;
	return RoarMontage && AnimInstance && AnimInstance->Montage_IsPlaying(RoarMontage);
}

void UBossLostTargetComponent::StartRoar()
{
	ABossEnemy* Owner = Boss.Get();

	LostTargetState = EBossLostTargetState::Roaring;
	Owner->NotifyLostTargetRoarStarted();

	// 이 위치를 찾아봤는데 없었다. 이후 마지막 목격 위치가 바뀌면 새 단서로 본다.
	const UEnemyDetectionComponent* Detection = Owner->GetEneymDetectionComponent();
	SearchedLocation = Detection->HasLastKnownLocation()
		? Detection->GetLastKnownLocation()
		: Owner->GetActorLocation();

	Owner->StopHorizontalMovement();

	if (RoarSound)
	{
		RoarAudio = UGameplayStatics::SpawnSoundAttached(RoarSound, Owner->GetRootComponent());
	}

	UAnimInstance* AnimInstance = Owner->GetMesh()->GetAnimInstance();
	const float RoarLength = RoarMontage && AnimInstance
		? AnimInstance->Montage_Play(RoarMontage)
		: 0.0f;

	UE_LOG(LogTemp, Log, TEXT("%s: 대상을 놓쳐 포효 (%.1f초)"), *Owner->GetName(), RoarLength);

	// 포효 몽타주가 없으면 바로 난사한다.
	if (RoarLength <= 0.0f)
	{
		StartRampage();
	}
}

void UBossLostTargetComponent::StartRampage()
{
	const double Now = GetWorld()->GetTimeSeconds();

	LostTargetState = EBossLostTargetState::Rampaging;
	RampageEndTime = Now + RampageDuration;
	NextPatternTime = Now;
	bWaitingPatternEnd = false;
	bHasDestination = false;
	bFacingChosen = false;

	UE_LOG(LogTemp, Log, TEXT("%s: 마지막 목격 위치 주변 난사 시작 (%.1f초)"), *GetOwner()->GetName(), RampageDuration);
}

void UBossLostTargetComponent::UpdateRampage(float DeltaTime)
{
	ABossEnemy* Owner = Boss.Get();
	const double Now = GetWorld()->GetTimeSeconds();

	// 시전 중인 패턴과 피격 반응은 끝날 때까지 기다린다. 패턴의 이동(돌진 등)을 막지 않도록 이동도 건드리지 않는다.
	if (Owner->IsAttackActive() || Owner->IsPlayingHitReaction())
	{
		bWaitingPatternEnd = bWaitingPatternEnd || Owner->IsAttackActive();
		return;
	}

	// 패턴이 끝난 순간부터 난사 전용 휴식 시간을 센다.
	if (bWaitingPatternEnd)
	{
		bWaitingPatternEnd = false;
		NextPatternTime = Now + FMath::FRandRange(
			PatternRestTimeMin,
			FMath::Max(PatternRestTimeMin, PatternRestTimeMax)
		);
	}

	if (Now >= RampageEndTime)
	{
		EndLostTarget(true);
		return;
	}

	// 다음 패턴을 쓸 지점을 마지막 목격 위치 좌우 RampageRadius 안에서 고른다.
	if (!bHasDestination)
	{
		const UEnemyDetectionComponent* Detection = Owner->GetEneymDetectionComponent();
		const FVector Center = Detection->HasLastKnownLocation()
			? Detection->GetLastKnownLocation()
			: Owner->GetActorLocation();

		DestinationX = Center.X + FMath::FRandRange(-RampageRadius, RampageRadius);
		bHasDestination = true;
		PreviousX = Owner->GetActorLocation().X;
		BlockedTime = 0.0f;
	}

	// 지점까지 이동한다. 휴식 시간은 이동하는 동안에도 흐르고, 오래 막혀 있으면 지금 위치에서 패턴을 쓴다.
	if (MoveBossTowardX(DestinationX, DeltaTime))
	{
		return;
	}

	Owner->StopHorizontalMovement();

	if (Now < NextPatternTime)
	{
		return;
	}

	// 대상이 어디 있는지 모르므로 좌우 중 아무 쪽이나 보고 쓴다.
	if (!bFacingChosen)
	{
		Owner->FaceDirection(FMath::RandBool() ? 1.0f : -1.0f);
		bFacingChosen = true;
		return;
	}

	if (Owner->IsTurning())
	{
		return;
	}

	if (ActivateRampagePattern())
	{
		bWaitingPatternEnd = true;
		bHasDestination = false;
		bFacingChosen = false;
		return;
	}

	NextPatternTime = Now + BossLostTarget::RetryDelay;
}

bool UBossLostTargetComponent::MoveBossTowardX(float TargetX, float DeltaTime)
{
	ABossEnemy* Owner = Boss.Get();
	const float CurrentX = Owner->GetActorLocation().X;
	if (FMath::Abs(TargetX - CurrentX) <= BossLostTarget::ArrivalDistance)
	{
		return false;
	}

	if (DeltaTime <= 0.0f)
	{
		return true;
	}

	// 기대 이동량의 일정 비율보다 적게 움직이면 막힌 것으로 본다. 제자리 회전 시간은 포기 시간보다 짧다.
	const float MovedDistance = FMath::Abs(CurrentX - PreviousX);
	const float ExpectedDistance = Owner->GetAIChaseSpeed() * DeltaTime;
	PreviousX = CurrentX;
	BlockedTime = MovedDistance < ExpectedDistance * BossLostTarget::BlockedMoveRatio
		? BlockedTime + DeltaTime
		: 0.0f;

	if (BlockedTime >= BossLostTarget::BlockedGiveUpTime)
	{
		return false;
	}

	Owner->MoveAlongX(TargetX, Owner->GetAIChaseSpeed(), DeltaTime);
	return true;
}

bool UBossLostTargetComponent::ActivateRampagePattern()
{
	ABossEnemy* Owner = Boss.Get();
	const UDataTable* PatternTable = Owner->GetPatternTable();
	if (!PatternTable)
	{
		return false;
	}

	// 난사 가중치가 있고 GAS 쿨타임이 아닌 모든 행이 후보다.
	TArray<TPair<FName, const FBossPatternRow*>> Candidates;
	float TotalWeight = 0.0f;
	for (const TPair<FName, uint8*>& Pair : PatternTable->GetRowMap())
	{
		const FBossPatternRow* Row = reinterpret_cast<const FBossPatternRow*>(Pair.Value);
		if (!Row || !Row->AbilityClass || Owner->IsPatternAbilityOnCooldown(*Row))
		{
			continue;
		}

		const float Weight = GetRampagePatternWeight(Pair.Key);
		if (Weight <= 0.0f)
		{
			continue;
		}

		Candidates.Emplace(Pair.Key, Row);
		TotalWeight += Weight;
	}

	if (Candidates.IsEmpty())
	{
		return false;
	}

	// 0 ~ 후보 가중치 합 사이 값을 뽑아 누적 가중치가 그 값을 넘는 행을 고른다.
	const TPair<FName, const FBossPatternRow*>* Picked = &Candidates.Last();
	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const TPair<FName, const FBossPatternRow*>& Candidate : Candidates)
	{
		Roll -= GetRampagePatternWeight(Candidate.Key);
		if (Roll <= 0.0f)
		{
			Picked = &Candidate;
			break;
		}
	}

	// 근접 판정은 놓친 대상에게만 들어간다. 놓친 대상이 사라졌으면(회수된 분신 등) 플레이어를 대상으로 둔다.
	APawn* Target = LostTarget.Get();
	if (!Owner->IsValidAITarget(Target))
	{
		Target = UGameplayStatics::GetPlayerPawn(this, 0);
	}

	if (!Owner->ActivatePatternAgainst(*Picked->Value, Target))
	{
		return false;
	}

	const float Weight = GetRampagePatternWeight(Picked->Key);
	const FString Message = FString::Printf(
		TEXT("[보스 난사] %s (%s) - 가중치 %.1f / 합 %.1f (%.1f%%)"),
		*Picked->Value->DisplayName.ToString(),
		*Picked->Key.ToString(),
		Weight,
		TotalWeight,
		Weight / TotalWeight * 100.0f
	);

	UE_LOG(LogTemp, Log, TEXT("%s"), *Message);

	if (bPrintPatternOnScreen && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange, Message);
	}

	return true;
}

float UBossLostTargetComponent::GetRampagePatternWeight(FName RowName) const
{
	const float* Weight = RampagePatternWeights.Find(RowName);
	return Weight ? *Weight : RampageDefaultPatternWeight;
}

void UBossLostTargetComponent::EndLostTarget(bool bForgetTarget)
{
	LostTargetState = EBossLostTargetState::None;
	LostTarget.Reset();
	bWaitingPatternEnd = false;
	bHasDestination = false;
	bFacingChosen = false;

	ABossEnemy* Owner = Boss.Get();
	if (!bForgetTarget || !Owner)
	{
		return;
	}

	// 대상을 잊으면 감지가 일반 상태로 돌아가 BT가 순찰 분기로 넘어간다.
	Owner->StopHorizontalMovement();
	if (UEnemyDetectionComponent* Detection = Owner->GetEneymDetectionComponent())
	{
		Detection->ForgetTarget();
	}

	UE_LOG(LogTemp, Log, TEXT("%s: 난사 종료, 순찰로 복귀"), *Owner->GetName());
}

TArray<FName> UBossLostTargetComponent::GetPatternRowNameOptions() const
{
	const ABossEnemy* Owner = Cast<ABossEnemy>(GetOwner());
	const UDataTable* PatternTable = Owner ? Owner->GetPatternTable() : nullptr;
	return PatternTable ? PatternTable->GetRowNames() : TArray<FName>();
}
