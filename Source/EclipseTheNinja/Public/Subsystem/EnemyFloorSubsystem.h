#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/TimerHandle.h"
#include "EnemyFloorSubsystem.generated.h"

class AEnemyFloorVolume;
class UEnemyFloorActivationComponent;

/**
 * 플레이어가 있는 층을 주기적으로 확인하고, 등록된 적에게 활성/비활성 판정을 맡긴다.
 * 레벨에 층 볼륨이 없으면 모든 적이 항상 활성이라 다른 스테이지에는 영향이 없다.
 */
UCLASS()
class ECLIPSETHENINJA_API UEnemyFloorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 게임·PIE 월드에서만 만든다.
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// 층 갱신 타이머를 시작한다.
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// 층 갱신 타이머를 멈춘다.
	virtual void Deinitialize() override;

	// 층 볼륨 등록/해제. 볼륨의 BeginPlay/EndPlay에서 부른다.
	void RegisterVolume(AEnemyFloorVolume* Volume);
	void UnregisterVolume(AEnemyFloorVolume* Volume);

	// 적 활성화 컴포넌트 등록/해제. 컴포넌트의 BeginPlay/EndPlay에서 부른다.
	void RegisterEnemy(UEnemyFloorActivationComponent* Component);
	void UnregisterEnemy(UEnemyFloorActivationComponent* Component);

	/** 위치가 속한 층 번호. 어느 볼륨에도 없으면 INDEX_NONE. 겹친 곳에서는 PreferredFloor를 우선하고, 없으면 작은 볼륨을 고른다. */
	int32 FindFloorAt(const FVector& Location, int32 PreferredFloor = INDEX_NONE) const;

	/** 마지막으로 확인한 플레이어 층. 아직 모르면 INDEX_NONE. */
	int32 GetPlayerFloor() const { return PlayerFloor; }

private:
	// 플레이어 층을 갱신하고 등록된 적 전부를 다시 판정한다.
	void UpdateFloors();

	TArray<TWeakObjectPtr<AEnemyFloorVolume>> Volumes;
	TArray<TWeakObjectPtr<UEnemyFloorActivationComponent>> Enemies;

	// 볼륨 밖(계단 사이 등)에 있으면 마지막 층을 유지한다.
	int32 PlayerFloor = INDEX_NONE;

	FTimerHandle UpdateTimer;

	// 층 판정 주기(초). 위치 비교뿐이라 가볍다.
	static constexpr float UpdateInterval = 0.25f;
};
