#include "GAS/Ability/GAAerialAssassinate.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy.h"
#include "Character/PlayerCharacter.h"
#include "Component/AssassinateTargetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveVector.h"
#include "GameFramework/RootMotionSource.h"

bool UGAAerialAssassinate::CanAssassinateTarget(const APlayerCharacter* Player,
	const UAssassinateTargetComponent* Target, const AEnemy* Enemy) const
{
	// 적보다 높은 발판 위에 서 있거나 벽에 매달려 있을 때만 뛰어내린다. 높이 조건은 IsAerialApproachClear가 확인한다.
	if (!Player->GetCharacterMovement()->IsMovingOnGround() && !Player->IsWallAttached()) return false;
	return Target->IsAerialApproachClear(Player);
}

void UGAAerialAssassinate::StartApproach()
{
	APlayerCharacter* Player = Assassin.Get();
	AEnemy* Enemy = Victim.Get();
	if (!IsValid(Player) || !IsValid(Enemy) || !TargetComponent.IsValid()) { OnInterrupted(); return; }

	// 착지위치를 가져온다
	LandingLocation = TargetComponent->GetAerialLandingLocation(Player);

	// 적 위로 지나가거나 내려앉는 동안 적 캡슐에 막히지 않게 한다. 처치되면 적 충돌은 꺼진다.
	SetIgnoreVictimCollision(true);

	// 내려찍는 암살 몽타쥬 실행
	// 위치는 점프가 맡으므로 암살자 몽타주의 루트 모션은 끈다.
	WaitForImpact();
	if (!PlayAttackMontage()) return;
	SetAttackRootMotionEnabled(false);

	// 점프 시작 시간/ 점프 지속 기간을 런타임에 구하기
	ResolveJumpWindow();

	// 칼날이 피격자 몸통을 지나도록, 몽타주 시작부터 칼이 꽂히는(착지) 순간까지 피격자를 옆으로 천천히 옮긴다.
	SlideVictim(VictimSideOffset, 0.0f, ResolvedJumpStartTime + ResolvedJumpDuration);

	// 암살 대상 피격
	ExecuteVictim();
	if (!IsActive()) return;

	// 발판 위 준비 동작이 끝나는 시점에 뛰어내린다.
	if (ResolvedJumpStartTime <= KINDA_SMALL_NUMBER)
	{
		StartJump();
		return;
	}

	// 점프 시작 시간이 짧지 않으면 그 만큼 기다렸다가 StartJump
	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, ResolvedJumpStartTime);
	WaitTask->OnFinish.AddDynamic(this, &UGAAerialAssassinate::StartJump);
	WaitTask->ReadyForActivation();
}

void UGAAerialAssassinate::ResolveJumpWindow()
{
	ResolvedJumpStartTime = JumpStartTime;
	ResolvedJumpDuration = JumpDuration;
	if (!bDetectJumpFromRootMotion || !IsValid(ExecutionMontage)) return;

	// 몽타주 루트 모션으로 이동을 시작하는 시점과 착지 시점을 찾는다.
	// 발판 위에서 앞으로 디디며 속도가 붙은 채 뛰어내리므로, 높이가 아니라 루트가 처음 움직이는 시점부터 이동해야
	// 이륙 순간 속도가 끊기지 않는다. 가장 낮은 높이에 처음 닿은 시점이 착지다.
	const FAnimExtractContext Context;
	const float Length = ExecutionMontage->GetPlayLength();
	constexpr int32 SampleCount = 120;
	constexpr float Tolerance = 2.0f;

	TArray<FVector> Offsets;
	Offsets.Reserve(SampleCount + 1);
	float LowestHeight = 0.0f;
	for (int32 Index = 0; Index <= SampleCount; ++Index)
	{
		const float Time = Length * Index / SampleCount;
		const FVector Offset = ExecutionMontage->ExtractRootMotionFromTrackRange(0.0f, Time, Context).GetTranslation();
		Offsets.Add(Offset);
		LowestHeight = FMath::Min(LowestHeight, Offset.Z);
	}

	// 아래로 내려가는 루트 모션이 거의 없으면 설정값을 그대로 쓴다.
	if (LowestHeight > -50.0f) return;

	int32 TakeoffIndex = INDEX_NONE;
	int32 LandIndex = INDEX_NONE;
	for (int32 Index = 0; Index <= SampleCount; ++Index)
	{
		if (TakeoffIndex == INDEX_NONE && Offsets[Index].Size() > Tolerance)
		{
			TakeoffIndex = FMath::Max(0, Index - 1);
		}
		if (TakeoffIndex != INDEX_NONE && Offsets[Index].Z <= LowestHeight + Tolerance)
		{
			LandIndex = Index;
			break;
		}
	}
	if (TakeoffIndex == INDEX_NONE || LandIndex == INDEX_NONE || LandIndex <= TakeoffIndex) return;

	ResolvedJumpStartTime = Length * TakeoffIndex / SampleCount;
	ResolvedJumpDuration = Length * (LandIndex - TakeoffIndex) / SampleCount;
}

void UGAAerialAssassinate::StartJump()
{
	if (bEnding) return;
	if (!IsActive()) return;

	APlayerCharacter* Player = Assassin.Get();
	if (!IsValid(Player)) { OnInterrupted(); return; }

	// 중력 없이 점프 궤적만 따라가도록 Flying으로 바꾼다. 종료 시 부모가 원래 이동 모드로 되돌린다.
	Player->GetCharacterMovement()->SetMovementMode(MOVE_Flying);

	constexpr float MinJumpDuration = 0.05f;
	constexpr float LandingDeadlineRatio = 0.9f;

	// 몽타주가 먼저 끝나 어빌리티가 종료되지 않도록 몽타주 길이 안에서 착지한다.
	const float Duration = FMath::Max(MinJumpDuration, FMath::Min(ResolvedJumpDuration,
		ExecutionMontage->GetPlayLength() * LandingDeadlineRatio - ResolvedJumpStartTime));

	// 점프 태스크 실행. 몽타주 루트 모션의 진행 비율을 따라 착지 위치까지 이동한다.
	UAbilityTask_ApplyRootMotionMoveToForce* JumpTask = MoveAlongAttackRootMotion(ResolvedJumpStartTime, Duration, LandingLocation);
	if (!JumpTask) { OnInterrupted(); return; }
	JumpTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UGAAerialAssassinate::OnJumpLanded);
	JumpTask->OnTimedOut.AddDynamic(this, &UGAAerialAssassinate::OnJumpLanded);
	JumpTask->ReadyForActivation();
}

void UGAAerialAssassinate::OnJumpLanded()
{
	if (bEnding) return;
	if (!IsActive()) return;

	APlayerCharacter* Player = Assassin.Get();
	if (!IsValid(Player)) { OnInterrupted(); return; }

	// 발판·벽에 막혀 착지 위치에 닿지 못했으면 남은 연출을 멈춘다. 처치는 이미 시작 시점에 확정됐다.
	if (FVector::DistSquared(Player->GetActorLocation(), LandingLocation) > FMath::Square(30.0f))
	{
		OnInterrupted();
		return;
	}

	// 착지 후 찌르고 돌아서는 동안 앞뒤로 디디는 걸음은 몽타주 루트 모션으로 움직인다.
	HandOverToAttackRootMotion();
}
