#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AnimEnums.h"
#include "EnemyClimbEntranceComponent.generated.h"

class AEnemy;
class UAnimMontage;

/** 벽 뒤에서 기어올라 플레이 평면까지 걸어 나오는 등장 경로. 모두 캡슐 중심 위치다. */
USTRUCT(BlueprintType)
struct FEnemyClimbEntrance
{
	GENERATED_BODY()

	/** 가장자리 너머 아래, 기어오르기를 시작하는 위치 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entrance")
	FVector StartLocation = FVector::ZeroVector;

	/** 가장자리 위에 올라선 위치 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entrance")
	FVector LedgeLocation = FVector::ZeroVector;

	/** 걸어 나와 서는 플레이 평면 위 위치 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entrance")
	FVector PlayLocation = FVector::ZeroVector;
};

/** 등장 연출 단계 */
UENUM(BlueprintType)
enum class EClimbEntrancePhase : uint8
{
	// 등장 중이 아니다.
	None,
	// 숨은 채로 시작 시간을 기다린다.
	Waiting,
	// 가장자리를 붙잡고 기어오른다. 충돌이 꺼져 있다.
	Climbing,
	// 가장자리에서 플레이 평면까지 걸어 나온다.
	Walking
};

/**
 * 소환된 적이 배경 쪽 벽 뒤에서 기어올라 플레이 평면으로 걸어 나오게 하는 등장 연출.
 *
 * 흐름: Waiting(숨김) -> Climbing(오르기 몽타주 재생, 충돌 끄고 위치를 직접 옮김)
 *   -> Walking(이동 평면 제한을 풀고 Y로 걸어 나옴) -> 끝나면 AI를 다시 시작하고 대상과 바로 전투에 들어간다.
 * 연출 중 메시가 벽을 파고드는 정도는 허용한다. 끝난 뒤에는 일반 적과 같은 상태로 돌아간다.
 * 풀 반환(Deactivate)이나 사망 시에는 연출을 멈추고 바꾼 설정을 되돌린다.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class ECLIPSETHENINJA_API UEnemyClimbEntranceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyClimbEntranceComponent();

	// 등장 연출을 시작한다. Delay초 동안 숨어 있다가 기어오르고, 끝나면 CombatActor와 전투에 들어간다.
	void StartEntrance(const FEnemyClimbEntrance& InEntrance, AActor* InCombatActor, float Delay);

	// 등장 연출을 멈추고 바꾼 설정을 되돌린다. AI는 다시 시작하지 않는다.
	void CancelEntrance();

	/** 등장 연출 중인지 */
	UFUNCTION(BlueprintPure, Category = "AI|Entrance")
	bool IsEntering() const { return Phase != EClimbEntrancePhase::None; }

	// 풀 반환 시 진행 중인 연출을 멈춘다.
	virtual void Deactivate() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 기어오르기 몽타주. 루트 모션은 무시하고 위치는 이 컴포넌트가 옮긴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Entrance")
	TObjectPtr<UAnimMontage> ClimbMontage;

	/** 기어오르기 몽타주 재생 속도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Entrance", meta = (ClampMin = "0.01"))
	float ClimbPlayRate = 1.0f;

	/** 몽타주가 없을 때 기어오르는 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Entrance", meta = (ClampMin = "0.1", Units = "s"))
	float ClimbDurationWithoutMontage = 1.2f;

	/** 기어오르기 진행률 중 이 지점까지 가장자리 높이로 올라온다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Entrance", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ClimbRiseEndAlpha = 0.7f;

	/** 기어오르기 진행률 중 이 지점부터 가장자리 위로 넘어온다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Entrance", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float ClimbForwardStartAlpha = 0.5f;

	/** 기어오르기 몽타주가 끝나기 전 이 시간(초)만큼 일찍 걷기로 넘어가며 섞는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Entrance", meta = (ClampMin = "0.0", Units = "s"))
	float ClimbBlendOutTime = 0.25f;

	/** 가장자리에서 플레이 평면까지 걸어 나오는 속도(cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Entrance", meta = (ClampMin = "1.0", Units = "cm/s"))
	float WalkSpeed = 200.0f;

	/** 플레이 평면에 도착했다고 볼 Y 거리(cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Entrance", meta = (ClampMin = "1.0", Units = "cm"))
	float WalkArriveTolerance = 10.0f;

	/** 걸어 나오다 막혀도 플레이 평면으로 옮기고 끝내는 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Entrance", meta = (ClampMin = "0.1", Units = "s"))
	float WalkTimeout = 3.0f;

	/** 현재 등장 연출 단계 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Entrance")
	EClimbEntrancePhase Phase = EClimbEntrancePhase::None;

private:
	// 숨김을 풀고 오르기 몽타주를 재생한다.
	void BeginClimb();

	// 오르기 진행률에 맞춰 시작 위치에서 가장자리 위로 옮긴다. 끝나면 걷기로 넘어간다.
	void UpdateClimb();

	// 충돌과 걷기 이동을 되돌리고 플레이 평면 쪽으로 걷기 시작한다.
	void BeginWalk();

	// 플레이 평면 Y까지 걸어 나온다. 도착하거나 시간이 지나면 연출을 끝낸다.
	void UpdateWalk(float DeltaTime);

	// 플레이 평면에 맞춰 세우고 AI를 다시 시작한다.
	void FinishEntrance();

	// 연출 중 바꾼 충돌·이동·루트 모션 설정을 되돌린다.
	void RestoreOwnerState();

	// 기어오르는 동안 카메라 쪽(걸어 나올 방향)을 바라보게 한다.
	void FaceWalkDirection();

	UPROPERTY(Transient)
	TObjectPtr<AEnemy> OwnerEnemy;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CombatActor;

	FEnemyClimbEntrance Entrance;

	// 현재 단계에 들어온 뒤 지난 시간과, 기다리거나 기어오를 전체 시간(초)
	float PhaseElapsed = 0.0f;
	float PhaseDuration = 0.0f;

	// 걷는 동안 거의 움직이지 못한 누적 시간과 직전 Y 위치. 막히면 기다리지 않고 끝낸다.
	float WalkBlockedTime = 0.0f;
	float PrevWalkY = 0.0f;

	// 연출 전 설정. 끝나거나 멈출 때 되돌린다.
	ECollisionEnabled::Type SavedCapsuleCollision = ECollisionEnabled::QueryAndPhysics;
	TEnumAsByte<ERootMotionMode::Type> SavedRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
	bool bCollisionOverridden = false;
	bool bRootMotionOverridden = false;
	bool bPlaneConstraintOverridden = false;
};
