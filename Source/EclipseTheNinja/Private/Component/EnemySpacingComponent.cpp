#include "Component/EnemySpacingComponent.h"

#include "Character/Enemy.h"
#include "Component/EnemyDetectionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace EnemySpacing
{
	// 대상이 이 속도보다 느리면 멈춘 것으로 본다. 움직이는 대상에게는 물러나지 않고 버틴다.
	constexpr float TargetStillSpeed = 10.0f;

	// 대상 간격을 이만큼 넘게 침범당해야 물러나기 시작한다. 조금 겹친 정도로는 움찔거리지 않게 한다.
	constexpr float TargetBackAwayStartDepth = 15.0f;

	// 캡슐 가장자리에서 이만큼 앞의 바닥을 검사해 낭떠러지를 미리 알아챈다.
	constexpr float LedgeProbeDistance = 20.0f;

	// 바닥 검사 트레이스를 발밑 계단 높이보다 이만큼 더 깊게 쏜다.
	constexpr float LedgeProbeExtraDepth = 10.0f;

	// BeginPlay한 모든 적의 간격 컴포넌트. 매 프레임 월드의 적을 순회하지 않고 이 목록만 본다.
	// PIE 여러 창처럼 월드가 여럿일 수 있으므로 쓰는 쪽에서 같은 월드인지 확인한다.
	TArray<TWeakObjectPtr<UEnemySpacingComponent>> RegisteredComponents;
}

UEnemySpacingComponent::UEnemySpacingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemySpacingComponent::OnRegister()
{
	Super::OnRegister();

	OwnerEnemy = Cast<AEnemy>(GetOwner());
}

void UEnemySpacingComponent::BeginPlay()
{
	Super::BeginPlay();

	// 간격 유지를 끈 적(보스)도 다른 적에게는 길을 막는 이웃이므로 항상 등록한다.
	EnemySpacing::RegisteredComponents.AddUnique(this);
}

void UEnemySpacingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 나와 함께 이미 사라진 컴포넌트도 정리한다.
	EnemySpacing::RegisteredComponents.RemoveAllSwap(
		[this](const TWeakObjectPtr<UEnemySpacingComponent>& Registered)
		{
			return !Registered.IsValid() || Registered.Get() == this;
		}
	);

	Super::EndPlay(EndPlayReason);
}

void UEnemySpacingComponent::ResetSurround()
{
	SurroundSide = 0.0f;
	bCrossingSurroundSide = false;
	bPrioritizingAttack = false;
}

bool UEnemySpacingComponent::ShouldPrioritizeAttack(const AActor* Target) const
{
	if (AttackPriorityDistance <= 0.0f || !OwnerEnemy->IsAIAttackCooldownReady())
	{
		return false;
	}

	// 대상 근처에서만 우선한다. 멀리서 추격할 때까지 무시하면 처음부터 겹쳐 몰려온다.
	const float	DistanceX = FMath::Abs(Target->GetActorLocation().X - OwnerEnemy->GetActorLocation().X);

	return DistanceX <= AttackPriorityDistance;
}

bool UEnemySpacingComponent::IsLedgeAhead(float DirectionX) const
{
	// 공중에 있으면 낙하 중이므로 검사하지 않는다.
	if (!bAvoidLedges || FMath::IsNearlyZero(DirectionX) || !OwnerEnemy->GetCharacterMovement()->IsMovingOnGround())
	{
		return false;
	}

	const float	ProbeX = OwnerEnemy->GetActorLocation().X
		+ FMath::Sign(DirectionX) * (OwnerEnemy->GetCapsuleComponent()->GetScaledCapsuleRadius() + EnemySpacing::LedgeProbeDistance);

	return !HasGroundAtX(ProbeX);
}

bool UEnemySpacingComponent::HasGroundAtX(float X) const
{
	// 캡슐 중심 높이에서 발밑 계단 높이보다 조금 더 깊게 내려 쏜다. 그 안에 막히는 바닥이 없으면 낭떠러지다.
	const FVector	Start(X, OwnerEnemy->GetActorLocation().Y, OwnerEnemy->GetActorLocation().Z);
	const float		ProbeDepth = OwnerEnemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		+ OwnerEnemy->GetCharacterMovement()->MaxStepHeight
		+ EnemySpacing::LedgeProbeExtraDepth;
	const FVector	End = Start - FVector::UpVector * ProbeDepth;

	// 이동과 같은 채널을 써서 몬스터가 실제로 딛을 수 있는 바닥만 찾는다. 다른 몬스터·플레이어는 이 채널을 무시한다.
	FCollisionQueryParams	Params(SCENE_QUERY_STAT(EnemyLedgeProbe), false, OwnerEnemy);
	FHitResult				Hit;

	return GetWorld()->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		OwnerEnemy->GetCapsuleComponent()->GetCollisionObjectType(),
		Params
	);
}

float UEnemySpacingComponent::ApplyMonsterSpacing(float DestinationX)
{
	const float	MyX = OwnerEnemy->GetActorLocation().X;
	const float	Direction = FMath::Sign(DestinationX - MyX);

	if (FMath::IsNearlyZero(Direction))
	{
		bHeldBySpacing = false;
		return DestinationX;
	}

	float		LimitedX = DestinationX;
	bool		bLimited = false;

	for (const TWeakObjectPtr<UEnemySpacingComponent>& Registered : EnemySpacing::RegisteredComponents)
	{
		const AEnemy* Other = GetSpacingNeighbor(Registered.Get());

		if (!Other)
		{
			continue;
		}

		// 이동 방향 앞쪽에 있는 몬스터에게만 양보한다. 뒤쪽 몬스터는 뒤쪽에서 알아서 멈춘다.
		const float	OtherX = Other->GetActorLocation().X;

		if ((OtherX - MyX) * Direction <= 0.0f)
		{
			continue;
		}

		// 마주 오는 몬스터에게는 양보하지 않는다. 서로 양보하다 둘 다 멈추는 교착을 막고, 겹쳐 지나가게 둔다.
		if (Other->GetVelocity().X * Direction < -1.0f)
		{
			continue;
		}

		const float	StopX = OtherX - Direction * GetMonsterSpacingDistance(Other);

		if ((LimitedX - StopX) * Direction > 0.0f)
		{
			LimitedX = StopX;
			bLimited = true;
		}
	}

	if (!bLimited)
	{
		bHeldBySpacing = false;
		return DestinationX;
	}

	// 이미 멈출 위치에 닿았거나 지나쳤으면 뒤로 물러나지 않고 그 자리에 선다.
	const float	RemainingX = (LimitedX - MyX) * Direction;

	if (RemainingX <= AEnemy::MoveArriveTolerance)
	{
		bHeldBySpacing = true;
		return MyX;
	}

	// 간격 때문에 멈춰 있었다면 충분히 벌어질 때까지 기다린다.
	if (bHeldBySpacing && RemainingX < MonsterSpacingResumeDistance)
	{
		return MyX;
	}

	bHeldBySpacing = false;
	return LimitedX;
}

bool UEnemySpacingComponent::TrySeparateFromMonsters(float Speed, float DeltaTime, const AActor* FaceTarget)
{
	const float	MyX = OwnerEnemy->GetActorLocation().X;
	const float	MyRadius = OwnerEnemy->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const bool	bHasFaceTarget = IsValid(FaceTarget);
	const float	FaceTargetX = bHasFaceTarget ? FaceTarget->GetActorLocation().X : 0.0f;

	for (const TWeakObjectPtr<UEnemySpacingComponent>& Registered : EnemySpacing::RegisteredComponents)
	{
		const AEnemy* Other = GetSpacingNeighbor(Registered.Get());

		if (!Other)
		{
			continue;
		}

		const float	OtherX = Other->GetActorLocation().X;

		// 둘 중 한쪽만 비켜 선다. 대상이 있으면 대상에서 먼 쪽이, 거리가 같거나 대상이 없으면 UniqueID가 큰 쪽이 양보한다.
		const float	MyTargetDistance = FMath::Abs(FaceTargetX - MyX);
		const float	OtherTargetDistance = FMath::Abs(FaceTargetX - OtherX);
		const bool	bSameTargetDistance = FMath::IsNearlyEqual(MyTargetDistance, OtherTargetDistance, 1.0f);

		if (bHasFaceTarget && !bSameTargetDistance && MyTargetDistance < OtherTargetDistance)
		{
			continue;
		}

		if ((!bHasFaceTarget || bSameTargetDistance) && OwnerEnemy->GetUniqueID() < Other->GetUniqueID())
		{
			continue;
		}

		const float	OtherRadius = Other->GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float	SpacingDistance = GetMonsterSpacingDistance(Other);

		// 캡슐이 겹쳐 있을 때만 비켜 선다. 간격이 음수면 그만큼 겹친 것까지는 그대로 둔다.
		const float	SeparateThreshold = FMath::Min(MyRadius + OtherRadius, SpacingDistance - AEnemy::MoveArriveTolerance);

		if (FMath::Abs(OtherX - MyX) >= SeparateThreshold)
		{
			continue;
		}

		// 대상이 있으면 대상 반대쪽, 없으면 상대 반대쪽으로 비킨다. 기준 X가 같으면 바라보는 방향의 뒤쪽으로 비킨다.
		const float	AwayFromX = bHasFaceTarget && !FMath::IsNearlyEqual(FaceTargetX, MyX)
			? FaceTargetX
			: OtherX;
		const float	Direction = FMath::IsNearlyEqual(AwayFromX, MyX)
			? -FMath::Sign(OwnerEnemy->GetActorForwardVector().X)
			: FMath::Sign(MyX - AwayFromX);
		const float	TargetX = OtherX + Direction * SpacingDistance;
		const float	DistanceX = TargetX - MyX;

		// 대상 반대쪽 목표가 이미 지금 위치보다 대상 쪽이면 물러날 필요가 없다. 물러날 쪽이 낭떠러지여도 겹친 채로 둔다.
		if (DistanceX * Direction <= 0.0f || IsLedgeAhead(DistanceX))
		{
			continue;
		}

		// 대상이 있으면 대상을 바라본 채 뒷걸음질한다.
		OwnerEnemy->FaceDirection(bHasFaceTarget ? FaceTargetX - MyX : DistanceX);
		OwnerEnemy->GetCharacterMovement()->MaxWalkSpeed = FMath::Min(FMath::Max(1.0f, Speed), FMath::Abs(DistanceX) / DeltaTime);

		OwnerEnemy->AddMovementInput(FVector::ForwardVector, FMath::Sign(DistanceX));
		return true;
	}

	return false;
}

bool UEnemySpacingComponent::SeparateFromMonstersInAttackRange(const AActor* Target, float DeltaTime)
{
	if (!bKeepMonsterSpacing || DeltaTime <= 0.0f || !IsValid(Target))
	{
		return false;
	}

	// 대상과의 간격을 먼저 되찾고, 그다음 몬스터끼리 떨어진다.
	if (TryBackAwayFromTarget(Target, DeltaTime))
	{
		return true;
	}

	// 공격할 수 있으면 몬스터와 겹쳐 있어도 비켜 서지 않고 바로 공격한다. 비켜 서면 공격 거리 밖으로 밀려났다 다시 오기를 반복한다.
	if (ShouldPrioritizeAttack(Target))
	{
		return false;
	}

	return TrySeparateFromMonsters(OwnerEnemy->GetAIChaseSpeed(), DeltaTime, Target);
}

float UEnemySpacingComponent::UpdateCombatDestinationX(const AActor* Target)
{
	const float	MyX = OwnerEnemy->GetActorLocation().X;
	const float	TargetX = Target->GetActorLocation().X;
	float		DestinationX = TargetX;

	// 대상 캡슐과 겹치지 않도록 대상 바로 앞이 아니라 간격만큼 떨어진 지점을 목적지로 둔다.
	const float	TargetSpacing = bKeepMonsterSpacing
		? GetTargetSpacingDistance(Target)
		: 0.0f;

	DestinationX += FMath::Sign(MyX - TargetX) * TargetSpacing;

	// 이번 이동에서 몬스터 간격보다 공격하러 가는 것을 우선할지 정한다. MoveAlongX가 이 값을 본다.
	bPrioritizingAttack = bKeepMonsterSpacing && ShouldPrioritizeAttack(Target);

	if (!bKeepMonsterSpacing || !bSurroundTarget)
	{
		ResetSurround();
		return DestinationX;
	}

	SurroundSide = ChooseSurroundSide(Target);

	// 정한 쪽의 반대편에 있으면 건너가기 시작하고, 정한 쪽으로 건너갈 거리만큼 넘어가면 멈춘다.
	// 건너간 자리도 대상과 겹치지 않도록 대상 간격보다 가까이 서지 않는다.
	const float	CrossDistance = FMath::Max(SurroundCrossDistance, TargetSpacing);

	// 건너갈 자리에 바닥이 없으면(대상이 낭떠러지 끝에 있으면) 건너가지 않고 지금 쪽에 선다.
	const bool	bOppositeSide = (MyX - TargetX) * SurroundSide < 0.0f;

	if (bOppositeSide && bAvoidLedges && !HasGroundAtX(TargetX + SurroundSide * CrossDistance))
	{
		SurroundSide = -SurroundSide;
		bCrossingSurroundSide = false;
	}

	const float	SideOffset = (MyX - TargetX) * SurroundSide;

	if (SideOffset < 0.0f)
	{
		bCrossingSurroundSide = true;
	}
	else if (SideOffset >= CrossDistance - AEnemy::MoveArriveTolerance)
	{
		bCrossingSurroundSide = false;
	}

	if (bCrossingSurroundSide)
	{
		DestinationX = TargetX + SurroundSide * CrossDistance;
	}

	return DestinationX;
}

float UEnemySpacingComponent::GetTargetSpacingDistance(const AActor* Target) const
{
	const float	RadiusSum = OwnerEnemy->GetCapsuleComponent()->GetScaledCapsuleRadius() + Target->GetSimpleCollisionRadius();

	return FMath::Max(0.0f, RadiusSum + TargetSpacingGap);
}

bool UEnemySpacingComponent::TryBackAwayFromTarget(const AActor* Target, float DeltaTime)
{
	const FVector	MyLocation = OwnerEnemy->GetActorLocation();
	const FVector	TargetLocation = Target->GetActorLocation();

	// 대상이 점프 등으로 다른 높이에 있거나 움직이는 중이면 물러나지 않고 버틴다.
	// 다가오는 대상에게 매번 조금씩 물러나면 움찔거려 보인다.
	const bool	bOtherHeight = FMath::Abs(TargetLocation.Z - MyLocation.Z) >= OwnerEnemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const bool	bTargetMoving = FMath::Abs(Target->GetVelocity().X) > EnemySpacing::TargetStillSpeed;

	if (bOtherHeight || bTargetMoving)
	{
		bBackingAwayFromTarget = false;
		return false;
	}

	const float	SpacingDistance = GetTargetSpacingDistance(Target);
	const float	OffsetX = MyLocation.X - TargetLocation.X;
	const float	DistanceToTarget = FMath::Abs(OffsetX);

	// 많이 겹쳤을 때 물러나기 시작해, 간격을 다 되찾을 때까지 한 번에 물러난다.
	if (DistanceToTarget < SpacingDistance - EnemySpacing::TargetBackAwayStartDepth)
	{
		bBackingAwayFromTarget = true;
	}
	else if (DistanceToTarget >= SpacingDistance - AEnemy::MoveArriveTolerance)
	{
		bBackingAwayFromTarget = false;
	}

	if (!bBackingAwayFromTarget)
	{
		return false;
	}

	// 지금 서 있는 쪽으로 물러난다. X가 같으면 바라보는 방향의 뒤쪽으로 물러난다.
	const float	Direction = FMath::IsNearlyZero(OffsetX)
		? -FMath::Sign(OwnerEnemy->GetActorForwardVector().X)
		: FMath::Sign(OffsetX);
	const float	DistanceX = TargetLocation.X + Direction * SpacingDistance - MyLocation.X;

	// 뒤가 낭떠러지면 물러나지 않고 겹친 채로 버틴다.
	if (IsLedgeAhead(DistanceX))
	{
		bBackingAwayFromTarget = false;
		return false;
	}

	OwnerEnemy->FaceDirection(-Direction);
	OwnerEnemy->GetCharacterMovement()->MaxWalkSpeed = FMath::Min(FMath::Max(1.0f, OwnerEnemy->GetAIChaseSpeed()), FMath::Abs(DistanceX) / DeltaTime);

	OwnerEnemy->AddMovementInput(FVector::ForwardVector, FMath::Sign(DistanceX));
	return true;
}

const AEnemy* UEnemySpacingComponent::GetSpacingNeighbor(const UEnemySpacingComponent* Other) const
{
	// 다른 월드(PIE 다른 창)의 적은 보지 않는다.
	if (!IsValid(Other) || Other == this || Other->GetWorld() != GetWorld())
	{
		return nullptr;
	}

	const AEnemy* OtherEnemy = Other->OwnerEnemy;

	if (!IsValid(OtherEnemy) || OtherEnemy == OwnerEnemy || OtherEnemy->IsDead() || OtherEnemy->IsHidden())
	{
		return nullptr;
	}

	if (!OtherEnemy->HasActorBegunPlay() || OtherEnemy->GetCapsuleComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		return nullptr;
	}

	// 다른 높이의 발판에 있는 몬스터는 길을 막지 않는다.
	const float	HeightDiff = FMath::Abs(OtherEnemy->GetActorLocation().Z - OwnerEnemy->GetActorLocation().Z);

	if (HeightDiff >= OwnerEnemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())
	{
		return nullptr;
	}

	return OtherEnemy;
}

float UEnemySpacingComponent::GetMonsterSpacingDistance(const AEnemy* Other) const
{
	const float	RadiusSum = OwnerEnemy->GetCapsuleComponent()->GetScaledCapsuleRadius()
		+ Other->GetCapsuleComponent()->GetScaledCapsuleRadius();

	return FMath::Max(0.0f, RadiusSum + MonsterSpacingGap);
}

float UEnemySpacingComponent::ChooseSurroundSide(const AActor* Target) const
{
	const float	TargetX = Target->GetActorLocation().X;
	const float	MyDistance = FMath::Abs(OwnerEnemy->GetActorLocation().X - TargetX);
	const float	MySide = GetSurroundHomeSide(TargetX);

	int32	SameSideCount = 1;
	int32	OtherSideCount = 0;
	int32	SameSideBehindCount = 0;

	for (const TWeakObjectPtr<UEnemySpacingComponent>& Registered : EnemySpacing::RegisteredComponents)
	{
		const UEnemySpacingComponent* OtherSpacing = Registered.Get();
		const AEnemy* Other = GetSpacingNeighbor(OtherSpacing);

		// 같은 대상을 두고 자리를 정한 동료만 센다.
		if (!Other || FMath::IsNearlyZero(OtherSpacing->SurroundSide))
		{
			continue;
		}

		const UEnemyDetectionComponent* OtherDetection = Other->GetEneymDetectionComponent();

		if (!OtherDetection || OtherDetection->GetCurrentTarget() != Target)
		{
			continue;
		}

		if (OtherSpacing->GetSurroundHomeSide(TargetX) * MySide < 0.0f)
		{
			++OtherSideCount;
			continue;
		}

		++SameSideCount;

		// 같은 쪽에서 나보다 대상에서 먼 동료를 센다. 거리가 같으면 UniqueID가 큰 쪽을 뒤로 본다.
		const float	OtherDistance = FMath::Abs(Other->GetActorLocation().X - TargetX);
		const bool	bOtherBehind = FMath::IsNearlyEqual(OtherDistance, MyDistance, 1.0f)
			? Other->GetUniqueID() > OwnerEnemy->GetUniqueID()
			: OtherDistance > MyDistance;

		if (bOtherBehind)
		{
			++SameSideBehindCount;
		}
	}

	// 내 쪽이 두 명 이상 많으면 남는 수의 절반만큼 맨 뒤에서부터 반대편으로 보낸다.
	const int32	MoveCount = (SameSideCount - OtherSideCount) / 2;

	if (SameSideBehindCount < MoveCount)
	{
		return -MySide;
	}

	return MySide;
}

float UEnemySpacingComponent::GetSurroundHomeSide(float TargetX) const
{
	// 건너가는 중이면 이미 가려는 쪽에 속한 것으로 본다.
	if (bCrossingSurroundSide && !FMath::IsNearlyZero(SurroundSide))
	{
		return SurroundSide;
	}

	// 대상과 X가 같으면 바라보는 방향의 뒤쪽을 내 쪽으로 본다.
	const float	MyX = OwnerEnemy->GetActorLocation().X;

	if (FMath::IsNearlyEqual(MyX, TargetX))
	{
		return -FMath::Sign(OwnerEnemy->GetActorForwardVector().X);
	}

	return FMath::Sign(MyX - TargetX);
}
