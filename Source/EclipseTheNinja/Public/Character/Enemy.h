

#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "Enemy.generated.h"

class UStatAttributeSet;
class UWidgetComponent;
class UAnimMontage;
class UGABaseAttack;
struct FOnAttributeChangeData;

/** 적의 순찰, 추적, 공격, 사망 상태. */
UENUM(BlueprintType)
enum class EEnemyBehaviorState : uint8
{
	Idle,
	Patrol,
	Chase,
	Attack,
	Dead
};

/** 공통 전투 기능에 AI 행동, 피격/사망 처리, 머리 위 UI를 추가한다. */
UCLASS()
class ECLIPSETHENINJA_API AEnemy : public ABaseCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEnemy();


	UFUNCTION(BlueprintPure, Category = "Combat|Death")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "AI|Target")
	AActor* GetCombatTarget() const;

	UFUNCTION(BlueprintPure, Category = "AI|State")
	EEnemyBehaviorState GetBehaviorState() const { return BehaviorState; }

	UFUNCTION(BlueprintCallable, Category = "AI|Patrol")
	void StartPatrol();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	/** Overhead Widget 초기화 함수**/
	UFUNCTION(BlueprintCallable, Category = "UI|Overhead")
	void InitializeOverHeadWidget();

	/** 카메라 시선과 마주보도록(카메라 Forward의 반대 방향, 사이각 180도) 위젯 컴포넌트의 월드 회전을 갱신 */
	UFUNCTION(BlueprintCallable, Category = "UI|Overhead")
	virtual void UpdateOverheadWidgetRotation();

	// 체력 어트리뷰트가 변경되었을 때 호출될 함수
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	// 피격 몽타주 재생용 함수
	UFUNCTION(BlueprintCallable, Category = "Combat|Hit Reaction")
	void PlayHitReaction();

	// 사망 처리용 함수
	UFUNCTION(BlueprintCallable, Category = "Combat|Death")
	void Die();

	// 몽타주 블랜드아웃용 함수
	void OnDeathMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	// 상태기반 로직 처리용 함수
	void UpdateBehavior(float DeltaTime);

	// 순찰 상태 업데이트용 함수
	void UpdatePatrol(float DeltaTime);

	// 상태 업데이트 함수
	void SetBehaviorState(EEnemyBehaviorState NewState);

	// 타겟이 공격 범위에서 벗어날 때 호출되는 함수
	UFUNCTION(BlueprintCallable, Category = "AI|Target")
	void LoseCombatTarget();

	// 폰이 살아있는지 확인하는 함수
	UFUNCTION(BlueprintPure, Category = "AI|Target")
	bool IsLivingPlayer(const APawn* Player) const;
	
	// 전투범위에 들어있는지 확인하는 함수
	UFUNCTION(BlueprintPure, Category = "AI|Detection")
	bool IsInsideCombatArea(const FVector& Location) const;

	// 기본 공격 어빌리티가 현재 실행중인지 확인하는 함수
	UFUNCTION(BlueprintPure, Category = "Combat|Attack")
	bool IsAttackActive() const;

	// 피격 몽타주가 현재 재생 중인지 확인하는 함수
	UFUNCTION(BlueprintPure, Category = "Combat|Hit Reaction")
	bool IsPlayingHitReaction() const;

	// 수평 움직임을 종료하는 함수
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void StopHorizontalMovement();

	// 좌우 방향을 바라보게 만드는 함수
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void FaceDirection(float DirectionX);

	// 프레임마다 DeltaTime을 전달해 DestinationX 방향으로 이동 입력을 넣는다.
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void MoveAlongX(float DestinationX, float Speed, float DeltaTime);

protected:
	/** 시작 위치를 기준으로 한 X축 순찰 범위(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Patrol")
	float PatrolMinOffset = -300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Patrol")
	float PatrolMaxOffset = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Patrol", meta = (ClampMin = "0.0", Units = "s"))
	float PatrolWaitTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Patrol", meta = (ClampMin = "1.0"))
	float PatrolSpeed = 150.0f;

	/** 시야 가림 검사 없이 거리로 플레이어를 감지한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Detection", meta = (ClampMin = "1.0"))
	float DetectionRadius = 500.0f;

	/** 추적 유지 거리. 실행 시 감지 반경 이상으로 보정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Detection", meta = (ClampMin = "1.0"))
	float LoseTargetRadius = 650.0f;

	/** 시작 위치 중심의 전투 영역. 영역 밖 플레이어는 추적하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Detection", meta = (ClampMin = "1.0"))
	float CombatAreaRadius = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "1.0"))
	float ChaseSpeed = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "1.0"))
	float AttackDistance = 140.0f;

	/** 공격 시작 사이의 간격(초). 진행 중인 공격은 유지한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "0.1", Units = "s"))
	float AttackInterval = 3.f;



	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI")
	EEnemyBehaviorState BehaviorState = EEnemyBehaviorState::Patrol;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> CombatTarget;

	FVector BehaviorOrigin = FVector::ZeroVector;
	float PatrolMinX = 0.0f;
	float PatrolMaxX = 0.0f;
	bool bPatrolTowardsMax = true;
	float PatrolWaitRemaining = 0.0f;
	double NextAttackTime = 0.0;

	/** 사망 몽타주 종료 후 시체를 유지하는 시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Death", meta = (ClampMin = "0.0", Units = "s"))
	float DeathDespawnDelay = 3.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Death")
	bool bIsDead = false;

	FDelegateHandle HealthChangedDelegateHandle;


	//머리위에 표시할 위젯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OverHead")
	TObjectPtr<UWidgetComponent> OverHeadWidgetComponent;

	/** 위젯이 항상 카메라를 바라보도록 회전할지 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Overhead")
	bool bFaceCamera = true;

	/** 빌보드 회전 시 상하 기울기(Pitch)를 고정(0)할지 여부 (true면 수평 유지, false면 카메라 시선과 정확히 일치) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Overhead")
	bool bLockWidgetPitch = false;

	/** 빌보드 회전 시 좌우 기울기(Roll)를 고정(0)할지 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Overhead")
	bool bLockWidgetRoll = true;

};
