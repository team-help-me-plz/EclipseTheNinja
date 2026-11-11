#include "Level/BossGimmickStage.h"

#include "AbilitySystemComponent.h"
#include "Character/Enemy.h"
#include "Checkpoint/Checkpoint.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Level/BossGimmickPortal.h"
#include "LevelInstance/LevelInstanceLevelStreaming.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "ObjectPool/ObjectPoolSubsystem.h"
#include "TimerManager.h"

ABossGimmickStage::ABossGimmickStage()
{
#if WITH_EDITOR
	SetDesiredRuntimeBehavior(ELevelInstanceRuntimeBehavior::LevelStreaming);
#endif
}

ABossGimmickStage* ABossGimmickStage::FindByLinkId(const UWorld* World, FName LinkId)
{
	if (!World) return nullptr;
	if (LinkId.IsNone()) return nullptr;

	for (TActorIterator<ABossGimmickStage> It(World); It; ++It)
	{
		if (It->LinkId == LinkId)
		{
			return *It;
		}
	}

	return nullptr;
}

ABossGimmickStage* ABossGimmickStage::FindOwningStage(const AActor* Actor)
{
	if (!IsValid(Actor)) return nullptr;

	const UWorld* World = Actor->GetWorld();
	if (!World) return nullptr;

	const ULevelInstanceSubsystem* Subsystem = World->GetSubsystem<ULevelInstanceSubsystem>();
	if (!Subsystem) return nullptr;

	const ULevel* Level = Actor->GetLevel();
	while (ILevelInstanceInterface* Instance = Subsystem->GetOwningLevelInstance(Level))
	{
		AActor* InstanceActor = CastChecked<AActor>(Instance);
		if (ABossGimmickStage* Stage = Cast<ABossGimmickStage>(InstanceActor))
		{
			return Stage;
		}
		Level = InstanceActor->GetLevel();
	}

	return nullptr;
}

bool ABossGimmickStage::IsInitiallyVisible() const
{
	const UWorld* World = GetWorld();
	if (!World) return false;
	if (!World->IsGameWorld()) return true;

	return bLinkActive;
}

void ABossGimmickStage::BeginPlay()
{
	Super::BeginPlay();

	if (UCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
	{
		Checkpoints->OnRespawned.AddDynamic(this, &ABossGimmickStage::HandleRespawned);
	}
}

void ABossGimmickStage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
	{
		Checkpoints->OnRespawned.RemoveDynamic(this, &ABossGimmickStage::HandleRespawned);
	}

	if (ULevelStreamingLevelInstance* Streaming = GetLevelStreaming())
	{
		Streaming->OnLevelShown.RemoveDynamic(this, &ABossGimmickStage::HandleStageShown);
	}

	Super::EndPlay(EndPlayReason);
}

void ABossGimmickStage::OnLevelInstanceLoaded()
{
	Super::OnLevelInstanceLoaded();
	if (!GetWorld()->IsGameWorld()) return;

	ULevelStreamingLevelInstance* Streaming = GetLevelStreaming();
	if (!Streaming) return;

	Streaming->OnLevelShown.AddUniqueDynamic(this, &ABossGimmickStage::HandleStageShown);
	UpdateVisibility();
	if (Streaming->IsLevelVisible())
	{
		HandleStageShown();
	}
}

bool ABossGimmickStage::IsReadyForEntry() const
{
	if (!bLinkActive) return false;

	const ULevelStreamingLevelInstance* Streaming = GetLevelStreaming();
	return Streaming && Streaming->IsLevelVisible();
}

void ABossGimmickStage::SetLinkActive(bool bActive)
{
	if (bLinkActive == bActive) return;

	bLinkActive = bActive;
	if (!bActive)
	{
		UpdateEnemies(false);
	}
	UpdateVisibility();
	if (IsReadyForEntry())
	{
		HandleStageShown();
	}
}

void ABossGimmickStage::SetPlayerInside(bool bInside)
{
	bPlayerInside = bInside;
	UpdateVisibility();
}

void ABossGimmickStage::UpdateVisibility()
{
	// 레버를 당긴 플레이어가 복귀 포탈에 도착할 때까지 방을 남겨 둔다.
	ULevelStreamingLevelInstance* Streaming = GetLevelStreaming();
	if (!Streaming) return;

	const bool bShow = bLinkActive || bPlayerInside;
	const ULevel* Room = Streaming->GetLoadedLevel();
	if (!bShow && Room && Streaming->IsLevelVisible())
	{
		// 숨긴 레벨에서는 GetWorld()가 끊길 수 있으므로 예약된 퍼즐 콜백을 먼저 정리한다.
		FTimerManager& Timers = GetWorldTimerManager();
		for (AActor* Actor : Room->Actors)
		{
			if (IsValid(Actor))
			{
				Timers.ClearAllTimersForObject(Actor);
			}
		}
	}

	Streaming->SetShouldBeVisible(bShow);
}

void ABossGimmickStage::HandleStageShown()
{
	if (!bLinkActive)
	{
		UpdateEnemies(false);
		UpdateVisibility();
		return;
	}

	UpdateEnemies(true);

	for (TActorIterator<ABossGimmickPortal> It(GetWorld()); It; ++It)
	{
		if (It->GetLinkId() == LinkId && It->GetPortalSide() == EBossGimmickPortalSide::BossStage)
		{
			It->RefreshBossStageEntry();
		}
	}
}

void ABossGimmickStage::HandleRespawned(ECheckpointId CheckpointId)
{
	if (!bPlayerInside) return;

	const UCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UCheckpointSubsystem>();
	if (!Checkpoints) return;

	FTransform RespawnTransform;
	if (!Checkpoints->GetCheckpointTransformById(CheckpointId, RespawnTransform)) return;

	for (TActorIterator<ACheckpoint> It(GetWorld()); It; ++It)
	{
		if (It->GetCheckpointId() == CheckpointId && FindOwningStage(*It) == this)
		{
			if (!bLinkActive)
			{
				UpdateEnemies(false);
			}
			return;
		}
	}

	SetPlayerInside(false);
}

void ABossGimmickStage::UpdateEnemies(bool bEnableAI)
{
	if (bEnableAI && !bEnemiesSuspended) return;
	if (!GetLoadedLevel()) return;

	bEnemiesSuspended = !bEnableAI;

	// 소환된 적은 영구 레벨에 있을 수 있어 소환자의 스테이지도 확인한다.
	TArray<AEnemy*> SummonedEnemies;
	for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
	{
		AEnemy* Enemy = *It;
		if (Enemy->IsDead()) continue;

		if (FindOwningStage(Enemy) == this)
		{
			if (bEnableAI)
			{
				Enemy->ResumeAIAfterScriptedMove(nullptr);
			}
			else
			{
				Enemy->SuspendAIForScriptedMove();
				if (UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
				{
					ASC->CancelAllAbilities();
				}
			}
		}
		else if (!bEnableAI && FindOwningStage(Enemy->GetOwner()) == this)
		{
			SummonedEnemies.Add(Enemy);
		}
	}

	for (AEnemy* Enemy : SummonedEnemies)
	{
		UObjectPoolSubsystem::Release(Enemy);
	}
}
