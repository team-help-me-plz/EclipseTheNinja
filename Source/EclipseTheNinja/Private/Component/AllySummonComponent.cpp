#include "Component/AllySummonComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Character/Enemy.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "Component/EnemyClimbEntranceComponent.h"
#include "Component/EnemyDetectionComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Util/EclipseCollisionChannels.h"
#include "Kismet/GameplayStatics.h"
#include "ObjectPool/ObjectPoolSubsystem.h"

UAllySummonComponent::UAllySummonComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAllySummonComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerEnemy = Cast<AEnemy>(GetOwner());

	UObjectPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<UObjectPoolSubsystem>();
	if (!PoolSubsystem || !SummonClass || SummonCount <= 0)
	{
		return;
	}

	// 프로젝트 설정에 없는 풀이면 여기서 등록한다. 미리 생성은 아래에서 소환 수만큼만 요청한다.
	if (SummonPoolData && !PoolSubsystem->HasPool(SummonClass))
	{
		PoolSubsystem->RegisterPool(SummonPoolData, false);
	}

	// 소환할 적을 미리 풀에 만들어 두어 소환 순간 끊기지 않게 한다. 풀에 등록되지 않은 클래스면 무시된다.
	PoolSubsystem->RequestWarmupByClass(SummonClass, SummonCount);
}

bool UAllySummonComponent::WantsToSummon() const
{
	if (!OwnerEnemy || OwnerEnemy->IsDead())
	{
		return false;
	}

	// 소리치는 중에는 분기가 끊기지 않게 계속 true다.
	if (bSummonInProgress)
	{
		return true;
	}

	if (bHasSummoned || !SummonClass || SummonCount <= 0 || !SummonMontage)
	{
		return false;
	}

	const UEnemyDetectionComponent* Detection = OwnerEnemy->GetEneymDetectionComponent();
	if (!Detection || Detection->GetCurrentAlertLevel() != EEnemyAlertLevel::Combat)
	{
		return false;
	}

	return OwnerEnemy->IsValidAITarget(Detection->GetCurrentTarget());
}

APawn* UAllySummonComponent::GetSummonTarget() const
{
	const UEnemyDetectionComponent* Detection = OwnerEnemy
		? OwnerEnemy->GetEneymDetectionComponent()
		: nullptr;

	return Detection
		? Detection->GetCurrentTarget()
		: nullptr;
}

void UAllySummonComponent::ResetSummon()
{
	bHasSummoned = false;
	bSummonInProgress = false;
}

int32 UAllySummonComponent::SummonAllies(AActor* CombatActor)
{
	if (bHasSummoned || !OwnerEnemy || !SummonClass)
	{
		return 0;
	}

	// 스폰 중 재진입을 막는다. 한 마리도 못 불렀으면 끝에서 소환 기회를 되돌린다.
	bHasSummoned = true;

	const FVector OwnerLocation = OwnerEnemy->GetActorLocation();
	const float BackgroundDirectionY = GetBackgroundDirectionY();

	// 대상 쪽(대상이 없으면 바라보는 쪽)에 먼저 세운다.
	float Side = CombatActor
		? FMath::Sign(CombatActor->GetActorLocation().X - OwnerLocation.X)
		: FMath::Sign(OwnerEnemy->GetActorForwardVector().X);

	if (FMath::IsNearlyZero(Side))
	{
		Side = 1.0f;
	}

	int32 SpawnedCount = 0;
	for (int32 Index = 0; Index < SummonCount; ++Index)
	{
		// 지정한 간격을 다 쓰면 마지막 간격만큼 이어서 더 멀리 세운다.
		float Offset = 150.0f + Index * 130.0f;
		if (SummonOffsetsX.Num() > 0)
		{
			const int32 LastIndex = SummonOffsetsX.Num() - 1;
			const float LastStep = SummonOffsetsX.Num() > 1
				? SummonOffsetsX[LastIndex] - SummonOffsetsX[LastIndex - 1]
				: SummonOffsetsX[LastIndex];

			Offset = Index <= LastIndex
				? SummonOffsetsX[Index]
				: SummonOffsetsX[LastIndex] + LastStep * (Index - LastIndex);
		}

		// 대상 쪽에 바닥이 없으면 반대쪽에서 찾는다.
		FEnemyClimbEntrance Entrance;
		bool bFound = FindEntrance(OwnerLocation.X + Side * Offset, BackgroundDirectionY, Entrance);
		if (!bFound)
		{
			bFound = FindEntrance(OwnerLocation.X - Side * Offset, BackgroundDirectionY, Entrance);
		}

		if (!bFound)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: %d번째 동료의 소환 위치를 확보하지 못했습니다. 바닥, 착지 공간, 걸어 나올 경로를 확인하세요."), *OwnerEnemy->GetName(), Index);
			continue;
		}

		if (SpawnAlly(Entrance, CombatActor, SpawnedCount * SummonInterval))
		{
			++SpawnedCount;
		}
	}

	if (SpawnedCount == 0)
	{
		bHasSummoned = false;
	}

	return SpawnedCount;
}

AEnemy* UAllySummonComponent::SpawnAlly(const FEnemyClimbEntrance& InEntrance, AActor* CombatActor, float Delay) const
{
	// 앞서 소환한 동료나 이동 물체가 공간을 차지했으면 이번 자리는 사용하지 않는다.
	if (!IsEntranceClear(InEntrance)) return nullptr;

	UWorld* World = GetWorld();
	UObjectPoolSubsystem* PoolSubsystem = World->GetSubsystem<UObjectPoolSubsystem>();
	if (!PoolSubsystem)
	{
		return nullptr;
	}

	// 풀에 등록된 클래스면 풀에서 꺼내고, 아니면 새로 스폰한다.
	const FTransform SpawnTransform(FRotator::ZeroRotator, InEntrance.StartLocation);
	AEnemy* Ally = PoolSubsystem->SpawnActor<AEnemy>(
		SummonClass,
		SpawnTransform,
		OwnerEnemy.Get(),
		nullptr
	);

	if (!Ally)
	{
		return nullptr;
	}

	// 체크포인트 리스폰 때 풀에 돌려보낸다.
	if (UCheckpointSubsystem* CheckpointSubsystem = World->GetSubsystem<UCheckpointSubsystem>())
	{
		CheckpointSubsystem->RegisterRuntimeSpawnedActor(Ally);
	}

	// 등장 연출이 없는 적은 플레이 평면에 바로 세워 전투에 들어간다.
	UEnemyClimbEntranceComponent* EntranceComp = Ally->FindComponentByClass<UEnemyClimbEntranceComponent>();
	if (!EntranceComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: EnemyClimbEntranceComponent가 없어 등장 연출 없이 세웁니다."), *Ally->GetName());

		Ally->SetActorLocation(InEntrance.PlayLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Ally->ResumeAIAfterScriptedMove(CombatActor);
		return Ally;
	}

	EntranceComp->StartEntrance(InEntrance, CombatActor, Delay);
	return Ally;
}

bool UAllySummonComponent::FindEntrance(float X, float BackgroundDirectionY, FEnemyClimbEntrance& OutEntrance) const
{
	const AEnemy* AllyDefault = SummonClass->GetDefaultObject<AEnemy>();
	const UCapsuleComponent* AllyCapsule = AllyDefault
		? AllyDefault->GetCapsuleComponent()
		: nullptr;

	if (!AllyCapsule)
	{
		return false;
	}

	const float AllyHalfHeight = AllyCapsule->GetScaledCapsuleHalfHeight();
	const float AllyRadius = AllyCapsule->GetScaledCapsuleRadius();

	// 몬스터가 실제로 딛는 바닥만 찾도록 이동과 같은 채널을 쓴다.
	const UCapsuleComponent* OwnerCapsule = OwnerEnemy->GetCapsuleComponent();
	const ECollisionChannel Channel = OwnerCapsule->GetCollisionObjectType();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AllySummonEntrance), false, OwnerEnemy);
	FHitResult Hit;

	// 플레이 평면의 바닥 높이. 주인 발밑 위아래로 찾는다.
	const FVector OwnerLocation = OwnerEnemy->GetActorLocation();
	const float OwnerHalfHeight = OwnerCapsule->GetScaledCapsuleHalfHeight();
	const FVector FloorStart(X, OwnerLocation.Y, OwnerLocation.Z + OwnerHalfHeight);
	const FVector FloorEnd(X, OwnerLocation.Y, OwnerLocation.Z - OwnerHalfHeight * 3.0f);

	if (!GetWorld()->LineTraceSingleByChannel(Hit, FloorStart, FloorEnd, Channel, Params))
	{
		return false;
	}
	if (Hit.bStartPenetrating) return false;
	if (!AllyDefault->GetCharacterMovement()->IsWalkable(Hit)) return false;

	const float FloorZ = Hit.ImpactPoint.Z;
	const float ProbeZ = FloorZ + EdgeWallProbeHeight;

	// 실제 가장자리가 없으면 최대 탐색 거리 뒤의 지면 아래에서 등장 연출을 시작한다.
	float EdgeDistance = MaxEdgeSearchDistance;
	for (float Distance = EdgeSearchStep; Distance <= MaxEdgeSearchDistance; Distance += EdgeSearchStep)
	{
		const float PrevY = OwnerLocation.Y + BackgroundDirectionY * (Distance - EdgeSearchStep);
		const float Y = OwnerLocation.Y + BackgroundDirectionY * Distance;

		const bool bWallHit = GetWorld()->LineTraceSingleByChannel(
			Hit,
			FVector(X, PrevY, ProbeZ),
			FVector(X, Y, ProbeZ),
			Channel,
			Params
		);

		if (bWallHit)
		{
			EdgeDistance = FMath::Abs(Hit.ImpactPoint.Y - OwnerLocation.Y);
			break;
		}

		const bool bFloorHit = GetWorld()->LineTraceSingleByChannel(
			Hit,
			FVector(X, Y, ProbeZ),
			FVector(X, Y, FloorZ - EdgeFloorTolerance),
			Channel,
			Params
		);

		if (!bFloorHit)
		{
			EdgeDistance = Distance - EdgeSearchStep;
			break;
		}
	}

	const float StandZ = FloorZ + AllyHalfHeight + 2.0f;
	const float LedgeDistance = FMath::Max(0.0f, EdgeDistance - AllyRadius - LedgeInset);
	const float EdgeY = OwnerLocation.Y + BackgroundDirectionY * EdgeDistance;

	OutEntrance.PlayLocation = FVector(X, OwnerLocation.Y, StandZ);
	OutEntrance.LedgeLocation = FVector(X, OwnerLocation.Y + BackgroundDirectionY * LedgeDistance, StandZ);
	OutEntrance.StartLocation = FVector(
		X,
		EdgeY + BackgroundDirectionY * FMath::Max(ClimbStartBehindEdge, AllyRadius + 2.0f),
		StandZ - ClimbStartDepth
	);
	if (!IsEntranceClear(OutEntrance)) return false;

	if (bDrawSummonDebug)
	{
		DrawDebugLine(GetWorld(), OutEntrance.StartLocation, OutEntrance.LedgeLocation, FColor::Orange, false, 5.0f, 0, 2.0f);
		DrawDebugLine(GetWorld(), OutEntrance.LedgeLocation, OutEntrance.PlayLocation, FColor::Green, false, 5.0f, 0, 2.0f);
		DrawDebugSphere(GetWorld(), FVector(X, EdgeY, FloorZ), 15.0f, 8, FColor::Red, false, 5.0f);
	}

	return true;
}

bool UAllySummonComponent::IsEntranceClear(const FEnemyClimbEntrance& Entrance) const
{
	UWorld* World = GetWorld();
	if (!World) return false;
	if (!SummonClass) return false;

	const AEnemy* AllyDefault = SummonClass->GetDefaultObject<AEnemy>();
	if (!IsValid(AllyDefault)) return false;

	const UCapsuleComponent* Capsule = AllyDefault->GetCapsuleComponent();
	if (!IsValid(Capsule)) return false;

	const UCharacterMovementComponent* Movement = AllyDefault->GetCharacterMovement();
	if (!IsValid(Movement)) return false;

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AllySummonClearance), false);
	FCollisionResponseParams Response(Capsule->GetCollisionResponseToChannels());
	Response.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	Response.CollisionResponse.SetResponse(ECC_Monster, ECR_Ignore);
	const ECollisionChannel Channel = OwnerEnemy->GetCapsuleComponent()->GetCollisionObjectType();

	FCollisionObjectQueryParams Characters;
	Characters.AddObjectTypesToQuery(ECC_Pawn);
	Characters.AddObjectTypesToQuery(ECC_Monster);
	// 오르기 시작점은 충돌을 끄고 벽·지면을 통과하는 연출 구간이므로 검사하지 않는다.
	for (const FVector& Location : { Entrance.LedgeLocation, Entrance.PlayLocation })
	{
		if (World->OverlapBlockingTestByChannel(Location, FQuat::Identity, Channel, Shape, Params, Response)) return false;
		if (World->OverlapAnyTestByObjectType(Location, FQuat::Identity, Characters, Shape, Params)) return false;
	}

	// 캡슐 중심 아래의 걸을 수 있는 바닥을 확인한다. 좁은 발판도 일반 캐릭터처럼 허용한다.
	for (const FVector& Location : { Entrance.LedgeLocation, Entrance.PlayLocation })
	{
		const FVector Feet = Location - FVector(0, 0, HalfHeight);
		FHitResult FloorHit;
		if (!World->LineTraceSingleByChannel(FloorHit, Feet + FVector(0, 0, EdgeWallProbeHeight),
			Feet - FVector(0, 0, EdgeFloorTolerance), Channel, Params, Response)) return false;
		if (FloorHit.bStartPenetrating) return false;
		if (!Movement->IsWalkable(FloorHit)) return false;
	}

	FHitResult PathHit;
	if (World->SweepSingleByChannel(PathHit, Entrance.LedgeLocation, Entrance.PlayLocation,
		FQuat::Identity, Channel, Shape, Params, Response)) return false;
	return true;
}

float UAllySummonComponent::GetBackgroundDirectionY() const
{
	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager)
	{
		return 1.0f;
	}

	// 카메라에서 주인을 향하는 Y 방향이 화면 안쪽(배경 쪽)이다.
	const float DirectionY = FMath::Sign(OwnerEnemy->GetActorLocation().Y - CameraManager->GetCameraLocation().Y);
	return FMath::IsNearlyZero(DirectionY)
		? 1.0f
		: DirectionY;
}
