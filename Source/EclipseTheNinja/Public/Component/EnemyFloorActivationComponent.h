#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AITypes.h"
#include "EnemyFloorActivationComponent.generated.h"

class AEnemy;
class UActorComponent;

/**
 * 플레이어와 먼 층에 있는 적의 틱·애니메이션·AI·감지를 멈추고, 가까워지거나 소리를 들으면 다시 켠다.
 *
 * 다른 시스템(사망, 체크포인트 리셋, 풀, 암살·연출의 AI 정지)과 상태가 섞이지 않도록
 * 1) 지금 켜져 있는 것만 끄고 무엇을 껐는지 기록하며, 2) 깨어날 때는 기록한 것만 되돌린다.
 * 다른 시스템이 이미 AI를 멈춘 상태(정지·일시정지)이거나 전투·공격·낙하 중이면 잠들지 않는다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ECLIPSETHENINJA_API UEnemyFloorActivationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyFloorActivationComponent();

	// 플레이어 층을 받아 활성/비활성을 정한다. 서브시스템이 주기적으로 부른다.
	void Evaluate(int32 PlayerFloor);

	// 소리·피격·사망·연출처럼 바로 반응해야 할 때 깨운다. 진정되면 다음 판정에서 다시 잠든다.
	void WakeUp();

	// 체크포인트 리셋·풀 스폰의 초기화 시작에 부른다. 멈춘 것을 되돌리고 현재 위치를 기준 위치로 삼는다.
	void HandleOwnerReset();

	// 풀로 반환될 때 부른다. 기록만 되돌려 두면 이후 틱은 풀이 관리한다.
	void HandleReturnToPool();

	// 지금 멈춰 있는지
	UFUNCTION(BlueprintPure, Category = "Floor Activation")
	bool IsDormant() const { return bDormant; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 끄면 층과 상관없이 항상 활동한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor Activation")
	bool bUseFloorActivation = true;

	/** 플레이어 층과의 차이가 이 값 이하면 활동한다. 1이면 바로 위아래 층까지 활동한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor Activation", meta = (ClampMin = "0"))
	int32 ActiveFloorRange = 1;

private:
	// 지금 잠들어도 다른 상태를 망가뜨리지 않는지 확인한다.
	bool CanEnterDormancy() const;

	// 켜져 있는 것만 끄고 기록한다.
	void EnterDormancy();

	// 기록한 것만 되돌린다.
	void ExitDormancy();

	// 컴포넌트 틱이 켜져 있으면 끄고 기록한다.
	void SuspendComponentTick(UActorComponent* Component);

	UPROPERTY(Transient)
	TWeakObjectPtr<AEnemy> OwnerEnemy;

	// 층을 정하는 기준 위치. 순찰로 움직여도 층이 바뀌지 않도록 배치·리스폰 위치를 쓴다.
	FVector HomeLocation = FVector::ZeroVector;

	bool bDormant = false;

	// 잠들 때 끈 것들. 깨어날 때 이것만 되돌린다.
	TArray<TWeakObjectPtr<UActorComponent>> SuspendedComponents;
	bool bSuspendedOwnerTick = false;
	bool bSuspendedControllerTick = false;
	bool bPausedBrain = false;
	bool bPausedMove = false;
	FAIRequestID PausedMoveRequestId;
};
