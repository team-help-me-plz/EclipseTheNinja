#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossLostTargetComponent.generated.h"

class ABossEnemy;
class UAudioComponent;
class UEnemyDetectionComponent;
class UAnimMontage;
class USoundBase;

/** 전투 중 대상을 놓쳤을 때 보스가 진행하는 단계. */
UENUM(BlueprintType)
enum class EBossLostTargetState : uint8
{
	/** 대상을 보고 있거나 놓침 처리를 하지 않는 중 */
	None,
	/** 놓친 직후 마지막 목격 위치로 가는 중. 가는 도중 대상을 다시 보면 포효 없이 전투로 돌아간다. */
	Approaching,
	/** 놓친 직후 포효 몽타주 재생 중. 다른 행동은 끝날 때까지 기다린다. */
	Roaring,
	/** 마지막 목격 위치 주변에서 패턴을 난사하는 중 */
	Rampaging,
};

/**
 * 보스가 전투 중 대상을 놓쳤을 때의 행동을 맡는다. 일반 적처럼 마지막 목격 위치 주변을 수색하는 대신 아래 순서로 진행한다.
 * 1. 접근: 마지막 목격 위치까지 간다. 가는 도중 대상을 다시 보면 일반 전투로 돌아간다.
 * 2. 포효: 도착해도(또는 길이 막혀도) 못 찾으면 RoarMontage와 RoarSound를 재생한다. 대상을 다시 봐도 끝까지 재생한다.
 * 3. 난사: 마지막 목격 위치 좌우 RampageRadius 안을 오가며, 페이즈와 무관하게 모든 패턴을 RampagePatternWeights로 골라 쓴다.
 * 4. 복귀: RampageDuration이 지나면 대상을 잊고 순찰로 돌아간다. 그 전에 대상을 다시 보면 일반 전투로 돌아간다.
 * 포효·난사 중 숨은 대상에게 공격받아 마지막 목격 위치가 바뀌면, 난사를 멈추고 1번(접근)부터 다시 한다.
 *
 * 스스로 Tick하지 않고 BT 태스크(UBTTask_BossLostTarget)가 BeginLostTarget → UpdateLostTarget → StopLostTarget 순서로 구동한다.
 * 언제 놓침 처리를 할지는 BT가 정하고, 이 컴포넌트는 그 동안의 행동과 설정값만 맡는다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ECLIPSETHENINJA_API UBossLostTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossLostTargetComponent();

	/** 놓친 대상이 유효하면 마지막 목격 위치로 가기 시작한다. 시작하지 못하면 false. */
	bool BeginLostTarget();

	/** 포효·난사를 진행한다. 끝났거나(대상을 다시 봄, 난사 시간 종료 등) 진행할 수 없으면 false. */
	bool UpdateLostTarget(float DeltaTime);

	/** 대상을 잊지 않고 놓침 처리만 멈춘다. BT가 다른 분기로 넘어갈 때 호출한다. */
	void StopLostTarget();
	void InterruptLostTarget();

	/** 대상을 놓친 뒤의 진행 단계. */
	UFUNCTION(BlueprintPure, Category = "Lost Target")
	EBossLostTargetState GetLostTargetState() const { return LostTargetState; }

	/** 포효나 난사를 진행 중인지. 이 동안 보스는 놓친 대상을 잊지 않는다. */
	UFUNCTION(BlueprintPure, Category = "Lost Target")
	bool IsHandlingLostTarget() const { return LostTargetState != EBossLostTargetState::None; }

	/** 마지막 목격 위치로 가는 중인지. 보스는 이때 페이즈와 무관하게 달린다. */
	UFUNCTION(BlueprintPure, Category = "Lost Target")
	bool IsApproaching() const { return LostTargetState == EBossLostTargetState::Approaching; }

	/** 포효 몽타주가 재생 중인지. */
	UFUNCTION(BlueprintPure, Category = "Lost Target")
	bool IsRoaring() const;

	/** 난사 중인지. 원거리 패턴은 이때 대상 대신 바라보는 방향으로 쏜다. */
	UFUNCTION(BlueprintPure, Category = "Lost Target")
	bool IsRampaging() const { return LostTargetState == EBossLostTargetState::Rampaging; }

protected:
	virtual void BeginPlay() override;

private:
	/** 마지막 목격 위치로 달려가기 시작한다. 처음 놓쳤을 때와, 포효·난사 중 새 단서가 생겼을 때 쓴다. */
	void StartApproach();

	/** 포효를 시작할 때 찾아본 위치와 지금의 마지막 목격 위치가 달라졌는지. 은신한 채 공격받은 경우다. */
	bool HasNewClue(const UEnemyDetectionComponent& Detection) const;

	/** 포효 몽타주와 사운드를 재생한다. 몽타주가 없으면 바로 난사한다. */
	void StartRoar();

	/**
	 * 보스를 TargetX 쪽으로 ChaseSpeed로 움직인다. 이번 프레임에 이동했으면 true.
	 * 도착했거나 BlockedGiveUpTime 동안 막혀 있었으면 이동하지 않고 false. 목표를 바꿀 때는 PreviousX와 BlockedTime을 초기화한다.
	 */
	bool MoveBossTowardX(float TargetX, float DeltaTime);

	/** 마지막 목격 위치 주변 난사를 시작한다. */
	void StartRampage();

	/** 난사 중 이동·방향 전환·패턴 시전을 진행한다. */
	void UpdateRampage(float DeltaTime);

	/** 난사 패턴 하나를 가중치 랜덤으로 골라 시전한다. 페이즈·사용 거리·행 쿨타임은 보지 않는다. */
	bool ActivateRampagePattern();

	/** 난사용 패턴 가중치. RampagePatternWeights에 없는 행은 RampageDefaultPatternWeight를 쓴다. */
	float GetRampagePatternWeight(FName RowName) const;

	/** 놓침 처리를 끝낸다. bForgetTarget이면 대상을 잊고 순찰로 돌아간다. */
	void EndLostTarget(bool bForgetTarget);

	/** RampagePatternWeights 키 선택지. 보스 패턴 테이블의 행 이름을 보여 준다. */
	UFUNCTION()
	TArray<FName> GetPatternRowNameOptions() const;

protected:
	/** 전투 중 대상을 놓쳤을 때 재생하는 포효 몽타주. 보스 ABP의 DefaultSlot을 사용한다. 비우면 포효 없이 바로 난사한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lost Target|Roar")
	TObjectPtr<UAnimMontage> RoarMontage;

	/** 포효를 시작할 때 보스에 붙여 재생하는 사운드. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lost Target|Roar")
	TObjectPtr<USoundBase> RoarSound;

	/** 난사하는 시간(초). 포효가 끝난 뒤부터 세며, 지나면 진행 중인 패턴을 마치고 순찰로 돌아간다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lost Target|Rampage", meta = (ClampMin = "0.0", Units = "s"))
	float RampageDuration = 8.0f;

	/** 난사 중 오가는 범위. 마지막 목격 위치에서 좌우로 이 거리(cm) 안의 지점으로 이동하며 패턴을 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lost Target|Rampage", meta = (ClampMin = "0.0", Units = "cm"))
	float RampageRadius = 400.0f;

	/** 난사 중 패턴이 끝난 뒤 다음 패턴까지 쉬는 최소 시간(초). 보스의 PatternRestTimeMin 대신 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lost Target|Rampage", meta = (ClampMin = "0.0", Units = "s"))
	float PatternRestTimeMin = 0.3f;

	/** 난사 중 패턴이 끝난 뒤 다음 패턴까지 쉬는 최대 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lost Target|Rampage", meta = (ClampMin = "0.0", Units = "s"))
	float PatternRestTimeMax = 0.8f;

	/**
	 * 난사할 때 패턴 행별 선택 가중치. 키는 보스 패턴 테이블의 행 이름이다.
	 * 여기에 없는 행은 RampageDefaultPatternWeight를 쓰고, 0이면 난사에서 쓰지 않는다.
	 * 난사는 페이즈 가중치(PhaseWeights)·사용 거리·행 쿨타임을 보지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lost Target|Rampage", meta = (GetKeyOptions = "GetPatternRowNameOptions", ClampMin = "0.0"))
	TMap<FName, float> RampagePatternWeights = { { FName(TEXT("QuickDrawProjectile")), 3.0f } };

	/** RampagePatternWeights에 없는 패턴 행의 난사 가중치. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lost Target|Rampage", meta = (ClampMin = "0.0"))
	float RampageDefaultPatternWeight = 1.0f;

	/** 고른 난사 패턴을 화면에 표시할지. 출력 로그에는 항상 남긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lost Target|Rampage")
	bool bPrintPatternOnScreen = true;

	/** 대상을 놓친 뒤의 진행 단계. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lost Target")
	EBossLostTargetState LostTargetState = EBossLostTargetState::None;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<ABossEnemy> Boss;

	TWeakObjectPtr<UAudioComponent> RoarAudio;

	/** 포효를 시작할 때의 대상. 난사 패턴의 근접 판정 대상으로 쓴다. */
	TWeakObjectPtr<APawn> LostTarget;

	/** 가서 찾아봤지만 대상이 없었던 위치. 포효를 시작할 때의 마지막 목격 위치다. */
	FVector SearchedLocation = FVector::ZeroVector;

	/** 난사가 끝나는 월드 시간(초)과 다음 패턴을 쓸 수 있는 월드 시간(초) */
	double RampageEndTime = 0.0;
	double NextPatternTime = 0.0;

	/** 난사 패턴이 시전 중이었는지. 끝나는 순간부터 휴식 시간을 센다. */
	bool bWaitingPatternEnd = false;

	/** 다음 패턴을 쓸 지점과, 그 지점에서 바라볼 방향을 정했는지 */
	float DestinationX = 0.0f;
	bool bHasDestination = false;
	bool bFacingChosen = false;

	/** 이동이 막혔는지 판단하는 이전 X 좌표와 거의 움직이지 못한 누적 시간 */
	float PreviousX = 0.0f;
	float BlockedTime = 0.0f;
};
