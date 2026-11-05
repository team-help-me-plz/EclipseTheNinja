#include "Component/AllySummonComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Character/Enemy.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "Component/EnemyClimbEntranceComponent.h"
#include "Component/EnemyDetectionComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
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

	// 자리가 없어 하나도 못 불러도 다시 시도하지 않는다.
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
		const bool bFound = FindEntrance(OwnerLocation.X + Side * Offset, BackgroundDirectionY, Entrance)
			|| FindEntrance(OwnerLocation.X - Side * Offset, BackgroundDirectionY, Entrance);

		if (!bFound)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: %d번째 동료를 세울 바닥을 찾지 못했습니다."), *OwnerEnemy->GetName(), Index);
			continue;
		}

		if (SpawnAlly(Entrance, CombatActor, SpawnedCount * SummonInterval))
		{
			++SpawnedCount;
		}
	}

	return SpawnedCount;
}

AEnemy* UAllySummonComponent::SpawnAlly(const FEnemyClimbEntrance& InEntrance, AActor* CombatActor, float Delay) const
{
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
		nullptr,
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

	const float FloorZ = Hit.ImpactPoint.Z;
	const float ProbeZ = FloorZ + EdgeWallProbeHeight;

	// 배경 쪽으로 나아가며 막는 벽이나 바닥이 끊긴 곳을 찾는다. 못 찾으면 최대 거리를 가장자리로 쓴다.
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
		EdgeY + BackgroundDirectionY * ClimbStartBehindEdge,
		StandZ - ClimbStartDepth
	);

	if (bDrawSummonDebug)
	{
		DrawDebugLine(GetWorld(), OutEntrance.StartLocation, OutEntrance.LedgeLocation, FColor::Orange, false, 5.0f, 0, 2.0f);
		DrawDebugLine(GetWorld(), OutEntrance.LedgeLocation, OutEntrance.PlayLocation, FColor::Green, false, 5.0f, 0, 2.0f);
		DrawDebugSphere(GetWorld(), FVector(X, EdgeY, FloorZ), 15.0f, 8, FColor::Red, false, 5.0f);
	}

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
