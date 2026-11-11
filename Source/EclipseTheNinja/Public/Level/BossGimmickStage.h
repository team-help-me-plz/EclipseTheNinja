#pragma once

#include "Checkpoint/CheckpointId.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "BossGimmickStage.generated.h"

/** 시작 시 로드하고 보스 에너지 링크에 따라 표시하는 기믹 방. */
UCLASS()
class ECLIPSETHENINJA_API ABossGimmickStage : public ALevelInstance
{
	GENERATED_BODY()

public:
	ABossGimmickStage();

	static ABossGimmickStage* FindByLinkId(const UWorld* World, FName LinkId);
	static ABossGimmickStage* FindOwningStage(const AActor* Actor);

	bool IsReadyForEntry() const;
	void SetLinkActive(bool bActive);
	void SetPlayerInside(bool bInside);

	virtual bool IsInitiallyVisible() const override;
	virtual void OnLevelInstanceLoaded() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleStageShown();

	UFUNCTION()
	void HandleRespawned(ECheckpointId CheckpointId);

	void UpdateVisibility();
	void UpdateEnemies(bool bEnableAI);

	UPROPERTY(EditInstanceOnly, Category = "Gimmick Stage")
	FName LinkId;

	bool bLinkActive = false;
	bool bPlayerInside = false;
	bool bEnemiesSuspended = false;
};
