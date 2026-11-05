#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Checkpoint/CheckpointId.h"
#include "AssassinationDropComponent.generated.h"

class AEnemy;

/**
 * 소유한 적이 암살로 처치되면, 사망 연출이 끝나 적이 사라질 때(또는 bUseDropDelay면 DropDelay초 뒤) 그 자리에 액터(회복 아이템 등)를 떨어뜨린다.
 * 적의 OnDied / OnDeathFinished 알림만 사용하므로 AEnemy 쪽에는 드롭 관련 코드가 없다.
 * 떨어뜨린 액터는 오브젝트 풀로 스폰하고, 체크포인트 리스폰 시 정리되도록 등록한다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ECLIPSETHENINJA_API UAssassinationDropComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAssassinationDropComponent();

	/** 떨어뜨릴 액터 클래스를 지정한다. 소유 액터의 생성자에서 기본값을 정할 때 사용한다. */
	void SetDropActorClass(TSubclassOf<AActor> InDropActorClass) { DropActorClass = InDropActorClass; }

	/** 사망 후 지연 드롭 사용 여부와 지연 시간을 지정한다. 소유 액터의 생성자에서 기본값을 정할 때 사용한다. */
	void SetDropDelay(bool bInUseDropDelay, float InDropDelay)
	{
		bUseDropDelay = bInUseDropDelay;
		DropDelay = InDropDelay;
	}

	/** 드롭할 월드 Y를 고정한다. 암살 연출이 캡슐을 옆으로 옮기기 전 위치를 넘겨 이동 평면에 떨어뜨리게 한다. */
	void SetDropPlaneY(float InPlaneY) { DropPlaneY = InPlaneY; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 적이 사망했을 때 암살 처치 여부를 기록한다. */
	void HandleEnemyDied(AEnemy* Enemy, bool bByAssassination);

	/** 적이 사라지기 직전, 아직 떨어뜨리지 않은 드롭이 있으면 그 자리에 스폰한다. */
	void HandleEnemyDeathFinished(AEnemy* Enemy);

	/** 지연 시간이 끝나면 시체 위치에 드롭 액터를 스폰한다. */
	void HandleDropDelayElapsed();

	/** 대기 중인 드롭을 시체 위치에 스폰하고 대기 상태를 해제한다. */
	void SpawnPendingDrop();

	/** 대기 중인 드롭과 지연 타이머를 취소한다. */
	void CancelPendingDrop();

	/** 체크포인트 리스폰 시 아직 떨어뜨리지 않은 드롭을 취소한다. */
	UFUNCTION()
	void HandleCheckpointRespawned(ECheckpointId CheckpointId);

	/** 드롭 액터를 풀에서 스폰하고 체크포인트 런타임 액터로 등록한다. */
	void SpawnDropActor(const FVector& SpawnLocation);

protected:
	/** 암살로 처치되었을 때 떨어뜨릴 액터. 비워두면 떨어뜨리지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop")
	TSubclassOf<AActor> DropActorClass;

	/** 스폰 위치. 적 메시 원점(발밑) 기준 월드 오프셋 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop")
	FVector SpawnOffset = FVector(0.0f, 0.0f, 60.0f);

	/** 켜면 시체가 사라질 때가 아니라 사망(암살 시작) 후 DropDelay초 뒤에 떨어뜨린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop")
	bool bUseDropDelay = false;

	/** 사망(암살 시작)부터 드롭까지의 시간(초). 시체가 먼저 사라지면 사라지는 순간에 떨어뜨린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop", meta = (EditCondition = "bUseDropDelay", ClampMin = "0.0", Units = "s"))
	float DropDelay = 1.0f;

private:
	/** 이번 사망이 암살 처치여서 아직 떨어뜨려야 할 드롭이 남아 있는지 여부 */
	bool bPendingDrop = false;

	/** 사망 후 DropDelay를 세는 타이머 */
	FTimerHandle DropDelayTimer;

	/** SetDropPlaneY로 받은 드롭 Y. 없으면 메시 위치의 Y를 쓴다. */
	TOptional<float> DropPlaneY;
};
