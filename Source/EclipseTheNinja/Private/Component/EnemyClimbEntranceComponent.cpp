#include "Component/EnemyClimbEntranceComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UEnemyClimbEntranceComponent::UEnemyClimbEntranceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// 풀 반환 시 Deactivate가 불리도록 활성 상태로 시작한다.
	bAutoActivate = true;
}

void UEnemyClimbEntranceComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerEnemy = Cast<AEnemy>(GetOwner());
}

void UEnemyClimbEntranceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelEntrance();

	Super::EndPlay(EndPlayReason);
}

void UEnemyClimbEntranceComponent::Deactivate()
{
	CancelEntrance();

	Super::Deactivate();
}

void UEnemyClimbEntranceComponent::StartEntrance(const FEnemyClimbEntrance& InEntrance, AActor* InCombatActor, float Delay)
{
	if (!OwnerEnemy)
	{
		OwnerEnemy = Cast<AEnemy>(GetOwner());
	}

	if (!OwnerEnemy || OwnerEnemy->IsDead())
	{
		return;
	}

	// 이전 연출이 남아 있으면 설정부터 되돌린다.
	CancelEntrance();

	Entrance = InEntrance;
	CombatActor = InCombatActor;

	// BT와 감지를 멈추고, 벽을 통과해 오를 수 있게 충돌과 중력을 끈다.
	OwnerEnemy->SuspendAIForScriptedMove();

	UCapsuleComponent* Capsule = OwnerEnemy->GetCapsuleComponent();
	SavedCapsuleCollision = Capsule->BodyInstance.GetCollisionEnabled(false);
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bCollisionOverridden = true;

	UCharacterMovementComponent* Movement = OwnerEnemy->GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Flying);

	// 플레이 평면 밖(배경 쪽)을 지나오므로 이동 평면 제한을 푼다.
	if (Movement->bConstrainToPlane)
	{
		Movement->SetPlaneConstraintEnabled(false);
		bPlaneConstraintOverridden = true;
	}

	OwnerEnemy->SetActorLocation(Entrance.StartLocation, false, nullptr, ETeleportType::TeleportPhysics);
	FaceWalkDirection();

	SetComponentTickEnabled(true);

	// 여러 마리를 차례로 올려 보내도록 숨긴 채 기다린다.
	if (Delay > KINDA_SMALL_NUMBER)
	{
		OwnerEnemy->SetActorHiddenInGame(true);
		Phase = EClimbEntrancePhase::Waiting;
		PhaseElapsed = 0.0f;
		PhaseDuration = Delay;
		return;
	}

	BeginClimb();
}

void UEnemyClimbEntranceComponent::CancelEntrance()
{
	if (Phase == EClimbEntrancePhase::None)
	{
		return;
	}

	if (OwnerEnemy && ClimbMontage)
	{
		if (UAnimInstance* AnimInstance = OwnerEnemy->GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.0f, ClimbMontage);
		}
	}

	RestoreOwnerState();

	Phase = EClimbEntrancePhase::None;
	CombatActor.Reset();
	SetComponentTickEnabled(false);
}

void UEnemyClimbEntranceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerEnemy || OwnerEnemy->IsDead())
	{
		CancelEntrance();
		return;
	}

	PhaseElapsed += DeltaTime;

	switch (Phase)
	{
	case EClimbEntrancePhase::Waiting:
		if (PhaseElapsed >= PhaseDuration)
		{
			BeginClimb();
		}
		break;

	case EClimbEntrancePhase::Climbing:
		UpdateClimb();
		break;

	case EClimbEntrancePhase::Walking:
		UpdateWalk(DeltaTime);
		break;

	default:
		SetComponentTickEnabled(false);
		break;
	}
}

void UEnemyClimbEntranceComponent::BeginClimb()
{
	OwnerEnemy->SetActorHiddenInGame(false);

	Phase = EClimbEntrancePhase::Climbing;
	PhaseElapsed = 0.0f;
	PhaseDuration = ClimbDurationWithoutMontage;

	UAnimInstance* AnimInstance = OwnerEnemy->GetMesh()->GetAnimInstance();
	if (!ClimbMontage || !AnimInstance)
	{
		return;
	}

	// 위치는 진행률로 직접 옮기므로 몽타주 루트 모션은 무시한다.
	SavedRootMotionMode = AnimInstance->RootMotionMode;
	AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	bRootMotionOverridden = true;

	const float PlayRate = FMath::Max(0.01f, ClimbPlayRate);
	const float MontageDuration = AnimInstance->Montage_Play(
		ClimbMontage,
		PlayRate,
		EMontagePlayReturnType::Duration
	);

	// 끝나기 조금 전에 걷기로 넘어가 몽타주가 섞이며 빠지게 한다.
	if (MontageDuration > 0.0f)
	{
		PhaseDuration = FMath::Max(0.1f, MontageDuration - ClimbBlendOutTime);
	}
}

void UEnemyClimbEntranceComponent::UpdateClimb()
{
	const float Alpha = FMath::Clamp(PhaseElapsed / FMath::Max(0.01f, PhaseDuration), 0.0f, 1.0f);

	// 먼저 몸을 끌어올리고, 뒤쪽 구간에서 가장자리 위로 넘어온다.
	const float RiseAlpha = FMath::Clamp(Alpha / FMath::Max(0.05f, ClimbRiseEndAlpha), 0.0f, 1.0f);
	const float ForwardAlpha = FMath::Clamp(
		(Alpha - ClimbForwardStartAlpha) / FMath::Max(0.05f, 1.0f - ClimbForwardStartAlpha),
		0.0f,
		1.0f
	);

	FVector NewLocation = Entrance.StartLocation;
	NewLocation.Z = FMath::InterpEaseOut(Entrance.StartLocation.Z, Entrance.LedgeLocation.Z, RiseAlpha, 2.0f);
	NewLocation.Y = FMath::InterpEaseInOut(Entrance.StartLocation.Y, Entrance.LedgeLocation.Y, ForwardAlpha, 2.0f);

	OwnerEnemy->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.0f)
	{
		BeginWalk();
	}
}

void UEnemyClimbEntranceComponent::BeginWalk()
{
	OwnerEnemy->SetActorLocation(Entrance.LedgeLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// 루트 모션 무시는 걸어 나온 뒤에 푼다. 블렌드 아웃 중인 오르기 몽타주의 루트 모션이 걷기 입력을 덮어 제자리에 멈추기 때문이다.
	if (UAnimInstance* AnimInstance = OwnerEnemy->GetMesh()->GetAnimInstance())
	{
		if (ClimbMontage && AnimInstance->Montage_IsPlaying(ClimbMontage))
		{
			AnimInstance->Montage_Stop(ClimbBlendOutTime, ClimbMontage);
		}
	}

	// 가장자리 위에 올라섰으니 충돌과 걷기를 되돌린다. 이동 평면 제한은 도착한 뒤에 다시 켠다.
	if (bCollisionOverridden)
	{
		OwnerEnemy->GetCapsuleComponent()->SetCollisionEnabled(SavedCapsuleCollision);
		bCollisionOverridden = false;
	}

	// 직전 AI 이동이 남긴 속도와 관계없이 정해진 속도로 걸어 나온다.
	UCharacterMovementComponent* Movement = OwnerEnemy->GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Walking);
	Movement->MaxWalkSpeed = WalkSpeed;

	Phase = EClimbEntrancePhase::Walking;
	PhaseElapsed = 0.0f;
	PhaseDuration = WalkTimeout;
	WalkBlockedTime = 0.0f;
	PrevWalkY = OwnerEnemy->GetActorLocation().Y;
}

void UEnemyClimbEntranceComponent::UpdateWalk(float DeltaTime)
{
	const float CurrentY = OwnerEnemy->GetActorLocation().Y;
	const float DistanceY = Entrance.PlayLocation.Y - CurrentY;

	if (FMath::Abs(DistanceY) <= WalkArriveTolerance || PhaseElapsed >= PhaseDuration)
	{
		FinishEntrance();
		return;
	}

	// 0.5초 넘게 거의 못 움직이면 막힌 것으로 보고 남은 거리를 옮겨 끝낸다.
	if (FMath::Abs(CurrentY - PrevWalkY) < 0.5f)
	{
		WalkBlockedTime += DeltaTime;
	}
	else
	{
		WalkBlockedTime = 0.0f;
	}

	PrevWalkY = CurrentY;

	if (WalkBlockedTime > 0.5f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 걸어 나오다 막혀 플레이 평면으로 옮깁니다."), *OwnerEnemy->GetName());
		FinishEntrance();
		return;
	}

	OwnerEnemy->AddMovementInput(FVector::RightVector, FMath::Sign(DistanceY));
}

void UEnemyClimbEntranceComponent::FinishEntrance()
{
	// 남은 Y 오차만 없애 플레이 평면 위에 정확히 세운다.
	FVector FinalLocation = OwnerEnemy->GetActorLocation();
	FinalLocation.Y = Entrance.PlayLocation.Y;
	OwnerEnemy->SetActorLocation(FinalLocation, true, nullptr, ETeleportType::TeleportPhysics);

	OwnerEnemy->StopHorizontalMovement();
	RestoreOwnerState();

	AActor* Target = CombatActor.Get();
	Phase = EClimbEntrancePhase::None;
	CombatActor.Reset();
	SetComponentTickEnabled(false);

	// 대상을 바라본 채 일반 적으로 돌아가 바로 전투에 들어간다.
	if (Target)
	{
		OwnerEnemy->FaceLocation(Target->GetActorLocation());
	}

	OwnerEnemy->ResumeAIAfterScriptedMove(Target);
}

void UEnemyClimbEntranceComponent::RestoreOwnerState()
{
	if (!OwnerEnemy)
	{
		return;
	}

	if (bCollisionOverridden)
	{
		OwnerEnemy->GetCapsuleComponent()->SetCollisionEnabled(SavedCapsuleCollision);
		bCollisionOverridden = false;
	}

	if (bRootMotionOverridden)
	{
		if (UAnimInstance* AnimInstance = OwnerEnemy->GetMesh()->GetAnimInstance())
		{
			AnimInstance->SetRootMotionMode(SavedRootMotionMode);
		}

		bRootMotionOverridden = false;
	}

	UCharacterMovementComponent* Movement = OwnerEnemy->GetCharacterMovement();
	if (bPlaneConstraintOverridden)
	{
		Movement->SetPlaneConstraintEnabled(true);
		bPlaneConstraintOverridden = false;
	}

	if (Movement->MovementMode == MOVE_Flying)
	{
		Movement->SetDefaultMovementMode();
	}
}

void UEnemyClimbEntranceComponent::FaceWalkDirection()
{
	const float DirectionY = Entrance.PlayLocation.Y - Entrance.StartLocation.Y;
	if (FMath::IsNearlyZero(DirectionY))
	{
		return;
	}

	// +Y로 걸어 나오면 Yaw 90, -Y면 -90을 바라본다.
	const float Yaw = DirectionY > 0.0f
		? 90.0f
		: -90.0f;

	OwnerEnemy->SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
}
