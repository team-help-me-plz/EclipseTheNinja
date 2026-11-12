#include "Subsystem/EnemyFloorSubsystem.h"

#include "Component/EnemyFloorActivationComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Level/EnemyFloorVolume.h"
#include "TimerManager.h"

bool UEnemyFloorSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UEnemyFloorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	FTimerManagerTimerParameters TimerParameters;
	TimerParameters.bLoop = true;
	TimerParameters.bMaxOncePerFrame = true;
	InWorld.GetTimerManager().SetTimer(
		UpdateTimer,
		this,
		&UEnemyFloorSubsystem::UpdateFloors,
		UpdateInterval,
		TimerParameters
	);
}

void UEnemyFloorSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
	}

	Volumes.Reset();
	Enemies.Reset();

	Super::Deinitialize();
}

void UEnemyFloorSubsystem::RegisterVolume(AEnemyFloorVolume* Volume)
{
	if (!IsValid(Volume))
	{
		return;
	}

	Volumes.AddUnique(Volume);
}

void UEnemyFloorSubsystem::UnregisterVolume(AEnemyFloorVolume* Volume)
{
	Volumes.Remove(Volume);
}

void UEnemyFloorSubsystem::RegisterEnemy(UEnemyFloorActivationComponent* Component)
{
	if (!IsValid(Component))
	{
		return;
	}

	Enemies.AddUnique(Component);
}

void UEnemyFloorSubsystem::UnregisterEnemy(UEnemyFloorActivationComponent* Component)
{
	Enemies.Remove(Component);
}

int32 UEnemyFloorSubsystem::FindFloorAt(const FVector& Location, int32 PreferredFloor) const
{
	const AEnemyFloorVolume* BestVolume = nullptr;
	double BestVolumeSize = TNumericLimits<double>::Max();

	for (const TWeakObjectPtr<AEnemyFloorVolume>& VolumePtr : Volumes)
	{
		const AEnemyFloorVolume* Volume = VolumePtr.Get();
		if (!Volume || !Volume->ContainsLocation(Location))
		{
			continue;
		}

		// 계단처럼 볼륨이 겹친 곳에서는 지금 층을 유지해 경계에서 층이 오가지 않게 한다.
		if (PreferredFloor != INDEX_NONE && Volume->GetFloorIndex() == PreferredFloor)
		{
			return PreferredFloor;
		}

		const double VolumeSize = Volume->GetBoxVolume();
		if (VolumeSize < BestVolumeSize)
		{
			BestVolume = Volume;
			BestVolumeSize = VolumeSize;
		}
	}

	return BestVolume ? BestVolume->GetFloorIndex() : INDEX_NONE;
}

void UEnemyFloorSubsystem::UpdateFloors()
{
	Volumes.RemoveAll([](const TWeakObjectPtr<AEnemyFloorVolume>& Volume)
	{
		return !Volume.IsValid();
	});
	Enemies.RemoveAll([](const TWeakObjectPtr<UEnemyFloorActivationComponent>& Component)
	{
		return !Component.IsValid();
	});

	// 플레이어가 볼륨 밖이거나 리스폰 중이라 폰이 없으면 마지막 층을 유지한다.
	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const int32 FoundFloor = FindFloorAt(PlayerPawn->GetActorLocation(), PlayerFloor);
		if (FoundFloor != INDEX_NONE)
		{
			PlayerFloor = FoundFloor;
		}
	}

	// 판정 중에 등록 목록이 바뀌어도 안전하도록 복사본을 돈다.
	const TArray<TWeakObjectPtr<UEnemyFloorActivationComponent>> EnemiesCopy = Enemies;
	for (const TWeakObjectPtr<UEnemyFloorActivationComponent>& Component : EnemiesCopy)
	{
		if (UEnemyFloorActivationComponent* Activation = Component.Get())
		{
			Activation->Evaluate(PlayerFloor);
		}
	}
}
