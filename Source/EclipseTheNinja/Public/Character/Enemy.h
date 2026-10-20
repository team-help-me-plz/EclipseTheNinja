

#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "Interface/CheckpointResettable.h"
#include "Enemy.generated.h"

class UStatAttributeSet;
class UWidgetComponent;
class UAnimMontage;
class UGABaseAttack;
class UEnemyDetectionComponent;
class UCheckpointBoundComponent;
class UAssassinateTargetComponent;
struct FOnAttributeChangeData;

/** 적의 순찰, 추적, 공격, 사망 상태. */
UENUM(BlueprintType)
enum class EEnemyBehaviorState : uint8
{
	Idle,
	Patrol,
	Chase,
	Attack,
	Dead,
	Investigate
};

/** 적의 경계 수준 */
UENUM(BlueprintType)
enum class EEnemyAlertLevel : uint8
{
	Normal,
	Alert,
	Combat,
};

/** 공통 전투 기능에 AI 행동, 피격/사망 처리, 머리 위 UI를 추가한다. */
UCLASS()
class ECLIPSETHENINJA_API AEnemy : public ABaseCharacter, public ICheckpointResettable
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEnemy();


	UFUNCTION(BlueprintPure, Category = "Combat|Death")
	bool IsDead() const { return bIsDead; }
	void ReceiveAssassination(UAnimMontage* Montage);

	UFUNCTION(BlueprintPure, Category = "AI|Target")
	AActor* GetCombatTarget() const;

	UFUNCTION(BlueprintPure, Category = "AI|State")
	EEnemyBehaviorState GetBehaviorState() const { return BehaviorState; }

	UFUNCTION(BlueprintCallable, Category = "AI|Patrol")
	void StartPatrol();

	UFUNCTION(BlueprintCallable, Category = "AI|Patrol")
	FVector ChooseNextPatrolLocation();

	UFUNCTION(BlueprintPure, Category = "AI")
	bool IsReadyForAI() const { return HasActorBegunPlay() && !bIsDead; }

	// 감지/추적/공격 판정용. 생존한 플레이어인지만 확인하고 활동 영역은 검사하지 않는다.
	UFUNCTION(BlueprintPure, Category = "AI|Target")
	bool IsValidAITarget(AActor* Target) const;

	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	bool IsWithinAttackRange(AActor* Target) const;

	UFUNCTION(BlueprintPure, Category = "AI|Movement")
	float GetAIChaseSpeed() const { return ChaseSpeed; }

	UFUNCTION(BlueprintCallable, Category = "AI|State")
	void SetBehaviorState(EEnemyBehaviorState NewState);

	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	bool TryStartAIBaseAttack(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	void CancelAIBaseAttack();

	UFUNCTION(BlueprintCallable, Category = "AI|Movement")
	void FaceLocation(const FVector& Location);



	// 체크포인트 리스폰 1단계: 체력/사망/전투 상태, 충돌, 이동, 애니메이션, 감지 상태를 초기화한다.
	virtual void ResetToInitial_Implementation() override;

	// 체크포인트 리스폰 2단계: 블랙보드를 비우고 BT를 처음부터 다시 실행한다.
	virtual void OnAfterReset_Implementation() override;

	UEnemyDetectionComponent* GetEneymDetectionComponent() const { return EnemyDetectComponent; }

protected:
	// Called when the game starts or when spawned
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> PendingAssassinationMontage;

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

	virtual void PlayHitReaction() override;


	// 사망 처리용 함수
	UFUNCTION(BlueprintCallable, Category = "Combat|Death")
	void Die();

	// 사망 연출이 끝난 뒤 숨김 처리하는 함수 (리스폰을 위해 Destroy하지 않는다)
	void DeactivateAfterDeath();

	// AI 컨트롤러의 블랙보드를 비우고 BT를 처음부터 다시 실행하는 함수
	void RestartAI();

	// 플레이어 또는 분신이 살아있는지 확인한다. 기존 Blueprint 함수 이름은 유지한다.
	UFUNCTION(BlueprintPure, Category = "AI|Target")
	bool IsLivingPlayer(const APawn* Player) const;

public:
	// BehaviorOrigin을 중심으로 활동영역 안에 있는지 확인하는 함수
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
	/** 시야 감지와 Blackboard 갱신을 담당한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Detection")
	TObjectPtr<UEnemyDetectionComponent> EnemyDetectComponent;

	/** 시작 위치를 기준으로 한 X축 순찰 범위(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Patrol")
	float PatrolMinOffset = -300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Patrol")
	float PatrolMaxOffset = 300.0f;

	/** 시작 위치 중심의 활동 영역. 순찰 구간과 경계 수색 지점을 제한한다. 추적/공격은 제한하지 않는다. */
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
	double NextAttackTime = 0.0;

	/** 사망 몽타주 종료 후 시체를 유지하는 시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Death", meta = (ClampMin = "0.0", Units = "s"))
	float DeathDespawnDelay = 3.0f;

	/** 이 적이 속한 체크포인트 구간. 해당 체크포인트에서 플레이어가 리스폰하면 적도 리스폰된다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCheckpointBoundComponent> CheckpointBound;

	/** 암살의 타겟으로 지정 가능해지도록 해주는 컴포넌트 */
	/** 이 컴포넌트를 가진 액터는 암살의 대상이 될 수 있다*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAssassinateTargetComponent> AssassinateTargetComt;

	FTimerHandle DeathDeactivateTimer;

	// 사망 시 끄는 충돌 설정을 리스폰 때 되돌리기 위한 초기값
	ECollisionEnabled::Type InitialCapsuleCollision = ECollisionEnabled::QueryAndPhysics;
	ECollisionEnabled::Type InitialMeshCollision = ECollisionEnabled::NoCollision;

	// 암살 연출이 옮긴 메시 위치를 리스폰 때 되돌리기 위한 초기값
	FVector InitialMeshRelativeLocation = FVector::ZeroVector;



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
