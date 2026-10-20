#include "GAS/Ability/GABelowAssassinate.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy.h"
#include "Character/PlayerCharacter.h"
#include "Component/AssassinateTargetComponent.h"

bool UGABelowAssassinate::CanAssassinateTarget(const APlayerCharacter* Player,
	const UAssassinateTargetComponent* Target, const AEnemy* Enemy) const
{
	// 적이 선 발판보다 낮은 바닥에 서 있을 때만 올라간다. 높이·모서리·경로는 대상 컴포넌트가 확인한다.
	if (!Player->GetCharacterMovement()->IsMovingOnGround()) return false;
	return Target->IsBelowApproachClear(Player);
}

void UGABelowAssassinate::StartApproach()
{
	APlayerCharacter* Player = Assassin.Get();
	AEnemy* Enemy = Victim.Get();
	if (!IsValid(Player) || !IsValid(Enemy) || !TargetComponent.IsValid()) { OnInterrupted(); return; }

	// 벽에 바짝 붙어 있을 때만 발동되므로 현재 위치에서 바로 올라간다.
	UAssassinateTargetComponent::FBelowPlan Plan;
	if (!TargetComponent->FindBelowPlan(Player, Plan)) { OnInterrupted(); return; }
	ClimbLocation = Plan.ClimbLocation;
	GrabLocation = Plan.GrabLocation;
	VictimForwardPush = Plan.VictimForwardPush;

	// 적 뒤로 다가가는 동안 적 캡슐에 막히지 않게 한다. 처치되면 적 충돌은 꺼진다.
	SetIgnoreVictimCollision(true);

	// 올라가기·다가가기는 코드가 이동시키므로 몽타주 루트 모션은 끈다.
	if (!PlayAttackMontage()) return;
	SetAttackRootMotionEnabled(false);

	ResolveClimbEndTime();

	// 벽을 타고 올라가는 동안 중력 없이 경로만 따라가도록 Flying으로 바꾼다.
	Player->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	UAbilityTask_ApplyRootMotionMoveToForce* ClimbTask = MoveAlongAttackRootMotion(0.0f, ResolvedClimbEndTime, ClimbLocation);
	if (!ClimbTask) { OnInterrupted(); return; }
	ClimbTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UGABelowAssassinate::OnClimbFinished);
	ClimbTask->OnTimedOut.AddDynamic(this, &UGABelowAssassinate::OnClimbFinished);
	ClimbTask->ReadyForActivation();

	// 올라가기 끝 자세(선 자세)와 목 꺾기 첫 자세(웅크린 자세)가 달라 구간 경계에서 끊긴다.
	// 경계보다 몽타주 Blend In 시간만큼 먼저 목 꺾기 구간을 새로 재생해 두 자세를 섞는다.
	CrossfadeTime = FMath::Clamp(ExecutionMontage->BlendIn.GetBlendTime(), 0.0f, ResolvedClimbEndTime);
	const float CrossfadeStart = ResolvedClimbEndTime - CrossfadeTime;
	if (CrossfadeStart <= KINDA_SMALL_NUMBER)
	{
		OnSectionCrossfade();
		return;
	}
	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, CrossfadeStart);
	WaitTask->OnFinish.AddDynamic(this, &UGABelowAssassinate::OnSectionCrossfade);
	WaitTask->ReadyForActivation();
}

void UGABelowAssassinate::OnSectionCrossfade()
{
	if (bEnding) return;
	if (!IsActive()) return;

	// 목 꺾기 구간부터 다시 재생한다. 위치는 계속 코드가 맡으므로 새 재생의 루트 모션도 끈다.
	// 다시 재생하면 몽타주 위치가 섞기 시간만큼 앞으로 뛰므로, 임팩트 대기는 새 재생 위치를 기준으로 건다.
	WaitForImpact(ResolvedClimbEndTime);
	if (!RestartAttackMontageAt(ResolvedClimbEndTime)) return;
	SetAttackRootMotionEnabled(false);

	// 목을 잡는 순간 손이 피격자 목에 오도록, 지금부터 목을 잡는 순간까지 피격자를 천천히 옮긴다.
	// 옆은 애니메이션 간격, 앞은 적이 모서리에 가까워 잡는 거리가 모자란 만큼이다.
	// 목 꺾기 구간을 방금 시작했으므로 잡는 순간까지는 GrabTime이 남았다.
	SlideVictim(VictimSideOffset, VictimForwardPush, GrabTime);

	// 페어 애니메이션이라 목 꺾기 구간과 함께 피격자 몽타주를 시작해 처치를 확정한다.
	ExecuteVictim();
}

void UGABelowAssassinate::ResolveClimbEndTime()
{
	ResolvedClimbEndTime = ClimbEndTime;
	if (!bDetectClimbEndFromMontage || !IsValid(ExecutionMontage)) return;
	if (ExecutionMontage->SlotAnimTracks.Num() == 0) return;

	// 두 번째 구간(목 꺾기)이 시작하는 시점이 올라가기가 끝나는 시점이다.
	const TArray<FAnimSegment>& Segments = ExecutionMontage->SlotAnimTracks[0].AnimTrack.AnimSegments;
	if (Segments.Num() < 2) return;
	ResolvedClimbEndTime = FMath::Max(0.05f, Segments[1].StartPos);
}

void UGABelowAssassinate::OnClimbFinished()
{
	if (bEnding) return;
	if (!IsActive()) return;

	APlayerCharacter* Player = Assassin.Get();
	if (!IsValid(Player)) { OnInterrupted(); return; }

	// 모서리에 걸려 올라서지 못했으면 처치하지 않는다.
	if (FVector::DistSquared(Player->GetActorLocation(), ClimbLocation) > FMath::Square(30.0f))
	{
		OnInterrupted();
		return;
	}

	// 발판 위에 섰으니 걷기로 돌아온다.
	Player->GetCharacterMovement()->StopMovementImmediately();
	Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	// 목 꺾기 구간은 섞기 시간만큼 먼저 시작했으므로, 그만큼 앞선 지점부터 목을 잡는 순간까지 적 뒤로 다가간다.
	const float WalkStartTime = ResolvedClimbEndTime + CrossfadeTime;
	const float WalkDuration = FMath::Max(0.05f, GrabTime - CrossfadeTime);
	UAbilityTask_ApplyRootMotionMoveToForce* WalkTask = MoveAlongAttackRootMotion(WalkStartTime, WalkDuration, GrabLocation);
	if (!WalkTask) { OnInterrupted(); return; }
	WalkTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UGABelowAssassinate::OnGrabReached);
	WalkTask->OnTimedOut.AddDynamic(this, &UGABelowAssassinate::OnGrabReached);
	WalkTask->ReadyForActivation();
}

void UGABelowAssassinate::OnGrabReached()
{
	if (bEnding) return;
	if (!IsActive()) return;

	APlayerCharacter* Player = Assassin.Get();
	if (!IsValid(Player)) { OnInterrupted(); return; }

	// 잡은 뒤에는 몽타주 루트 모션으로 뒤로 물러난다.
	HandOverToAttackRootMotion();
}
