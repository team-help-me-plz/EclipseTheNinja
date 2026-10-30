#pragma once

#include "CoreMinimal.h"
#include "Character/Enemy.h"
#include "Interface/Activatable.h"
#include "GameplayAbilitySpecHandle.h"
#include "Character/BossPatternRow.h"
#include "BossEnemy.generated.h"

class UGameplayAbility;
class UBossHealthBarWidget;
struct FAbilityEndedData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossEnergyLinkActivated, int32, LinkIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossEnergyLinkEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossPhaseChanged, int32, NewPhase);

/**
 * 비헤이비어 트리로 움직이는 보스 적.
 *
 * 감지·경계·추격·피격·체크포인트 리셋은 AEnemy의 일반 적 흐름을 그대로 사용한다.
 * 이 클래스는 보스 전용 규칙만 추가한다.
 * - 공격: BT의 Attack 태스크가 호출하면 PatternTable에서 쿨타임과 사용 거리를 통과한 패턴을 가중치 랜덤으로 골라 실행한다.
 *   패턴은 각각 UGABossPatternBase 계열 어빌리티이며, 테이블이 없으면 공격하지 않는다.
 * - 에너지 링크: 체력이 EnergyLinkThresholds 비율에 닿으면 그 값에서 체력을 멈추고 무적 실드를 켠다.
 *   실드는 레버(IActivatable) 또는 BreakEnergyLink 호출로 해제되고, 제한시간이 지나면 보스가 플레이어를 처형(암살)한다.
 * - 암살: 가장 가까운 아래쪽 에너지 링크 체력까지 즉시 깎는다. 체력이 AssassinationKillRatio 이하면 즉시 처치한다.
 * - 페이즈: 1페이즈에서 시작해 에너지 링크를 파훼할 때마다 1씩 오른다. 체력 구간이 아니라 파훼 횟수로 정한다.
 *   RunPhase 전에는 StanceWalkSpeed로 자세를 잡고 걷고, RunPhase부터 ChaseSpeed로 달린다.
 *   패턴 테이블의 PhaseWeights에서 지금 페이즈의 가중치로 패턴을 고르며, 가중치가 0인 페이즈에서는 그 패턴을 고르지 않는다.
 */
UCLASS()
class ECLIPSETHENINJA_API ABossEnemy : public AEnemy, public IActivatable
{
	GENERATED_BODY()

public:
	ABossEnemy();

	/** Also callable by an arena trigger/cinematic. Detection of combat starts the UI automatically. */
	UFUNCTION(BlueprintCallable, Category = "Boss|UI")
	void ShowBossHealthBar();

	/** 에너지 링크 실드가 켜져 있는지. */
	UFUNCTION(BlueprintPure, Category = "Boss|Energy Link")
	bool IsEnergyLinkActive() const { return bEnergyLinkActive; }

	/** Actual gimmick deadline in seconds. -1 means no active timed energy link; 0 means expired. */
	UFUNCTION(BlueprintPure, Category = "Boss|Energy Link")
	float GetEnergyLinkTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Boss|Energy Link")
	float GetEnergyLinkTimeLimit() const { return EnergyLinkTimeLimit; }

	/** 현재 체력 비율(0~1). */
	UFUNCTION(BlueprintPure, Category = "Boss")
	float GetHealthRatio() const;

	/** 실드를 해제한다. 레버가 ToggleActivate로 호출하며, 레벨 BP에서 직접 호출해도 된다. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Energy Link")
	void BreakEnergyLink();

	/** 현재 페이즈(1부터). 에너지 링크를 파훼할 때마다 1씩 오른다. */
	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	int32 GetPhase() const { return 1 + BrokenEnergyLinkCount; }

	/** 지금 페이즈에서 전투 이동을 달리기로 하는지. false면 자세를 잡고 걷는다. */
	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	bool IsRunPhase() const { return GetPhase() >= RunPhase; }

	/** 에너지 링크 파훼로 페이즈가 오르거나, 체크포인트 리셋으로 1페이즈로 돌아갈 때 호출된다. */
	UPROPERTY(BlueprintAssignable, Category = "Boss|Phase")
	FOnBossPhaseChanged OnPhaseChanged;

	/** 실드가 켜질 때 호출된다. LinkIndex는 EnergyLinkThresholds의 순번(0부터). */
	UPROPERTY(BlueprintAssignable, Category = "Boss|Energy Link")
	FOnBossEnergyLinkActivated OnEnergyLinkActivated;

	/** 공격 판정 디버그 그림(근접 판정 범위, 검기 발사 방향·히트박스)을 켜거나 끈다. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Debug")
	void SetDrawDebugAttack(bool bEnable);

	/** 공격 판정 디버그 그림이 켜져 있는지. */
	UFUNCTION(BlueprintPure, Category = "Boss|Debug")
	bool IsDrawDebugAttackEnabled() const { return bDrawDebugAttack; }

	/** 근접 판정 그림을 화면에 남겨 둘 시간(초). */
	float GetDebugAttackDrawDuration() const { return DebugAttackDrawDuration; }

	/** 레버 등으로 실드가 해제되면 호출된다. */
	UPROPERTY(BlueprintAssignable, Category = "Boss|Energy Link")
	FOnBossEnergyLinkEvent OnEnergyLinkBroken;

	/** 제한시간 안에 실드를 해제하지 못해 플레이어를 처치할 때 호출된다. */
	UPROPERTY(BlueprintAssignable, Category = "Boss|Energy Link")
	FOnBossEnergyLinkEvent OnEnergyLinkFailed;

	// 처형 어빌리티가 읽는 설정
	const TSoftObjectPtr<UAnimMontage>& GetExecutionAttackMontage() const { return ExecutionAttackMontage; }
	const TSoftObjectPtr<UAnimMontage>& GetExecutionVictimMontage() const { return ExecutionVictimMontage; }
	float GetExecutionBehindDistance() const { return ExecutionBehindDistance; }

	// 실드가 켜져 있으면 암살할 수 없다.
	virtual bool CanBeAssassinated() const override;

	// 체력이 낮으면 즉시 처치하고, 아니면 가장 가까운 아래쪽 에너지 링크 체력까지 깎는다.
	virtual void ReceiveAssassination(UAnimMontage* Montage) override;

	// 레버가 작동하면 실드를 해제한다.
	virtual void ToggleActivate_Implementation() override;

	// 체크포인트 리스폰 시 에너지 링크 진행도와 실드도 처음 상태로 되돌린다.
	virtual void ResetToInitial_Implementation() override;

	// 패턴 테이블이 있으면 지금 쓸 수 있는 패턴이 하나라도 있어야 공격 분기로 넘어간다.
	// 원거리 패턴은 근접 거리보다 먼 곳에서도 공격을 시작하고, 모두 쿨타임이면 추격을 계속한다.
	virtual bool CanStartAIAttack(AActor* Target) const override;

	// 패턴 사이 휴식 중에는 대상을 바라보며 PatternRestKeepDistance까지만 다가간다. 휴식이 아니면 일반 추격을 한다.
	virtual void UpdateCombatMovement(AActor* Target, float DeltaSeconds) override;

	// 패턴이 끝난 뒤의 휴식 시간(또는 활성화 실패 후 재시도 대기)이 남아 있는지.
	UFUNCTION(BlueprintPure, Category = "Boss|Pattern")
	bool IsRestingBetweenPatterns() const;

	// 원거리 패턴을 쓸 수 있도록 전투 중에는 발각 반경 대신 감지 반경(경계 배율 적용)까지 대상을 계속 본다.
	virtual float GetCombatSightRadius() const override;

	// TurnRate로 정한 속도로 서서히 돈다. 실제 회전은 Tick에서 진행한다.
	virtual void FaceDirection(float DirectionX) override;

	// RunPhase 전에는 추격·마지막 목격 위치 이동 등 모든 이동이 StanceWalkSpeed를 넘지 않는다.
	// 크게 도는 동안에는 뒷걸음질치지 않도록 제자리에서 다 돈 뒤 이동한다.
	virtual void MoveAlongX(float DestinationX, float Speed, float DeltaTime) override;

	// 방향 전환이 아직 진행 중인지
	UFUNCTION(BlueprintPure, Category = "Boss|Movement")
	bool IsTurning() const { return bTurning; }

	// 보스는 발도 준비 동작 중 대상이 움직여도 공격을 끝까지 수행한다.
	virtual bool ShouldLockMovementDuringAttack() const override { return true; }

	// 공격 도중 대상이 죽거나 사라져도 패턴 몽타주를 끝까지 재생한다.
	virtual bool ShouldFinishAttackWithoutTarget() const override { return true; }

	// 패턴 테이블로 고른 어빌리티가 실행 중인지. 아직 패턴을 쓰지 않았으면 기본 공격 입력 ID 어빌리티를 본다.
	virtual bool IsAttackActive() const override;

	// 실행 중인 패턴 어빌리티를 취소한다.
	virtual void CancelAIBaseAttack() override;

	// 보스는 패턴이 끝난 뒤부터 PatternRestTime으로 쉬므로, 패턴 시작 기준 간격(AttackInterval)은 쓰지 않는다.
	virtual float GetAIAttackCooldown() const override { return 0.1f; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|UI")
	TSubclassOf<UBossHealthBarWidget> BossHealthBarClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|UI")
	FText BossDisplayName = NSLOCTEXT("BossUI", "OniName", "오니 사무라이");

	UPROPERTY(Transient)
	TObjectPtr<UBossHealthBarWidget> BossHealthBar;

	void HideBossHealthBar();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	// 에너지 링크 체력에 닿았는지 검사한 뒤 일반 적의 피격/사망 처리로 넘긴다.
	virtual void OnHealthChanged(const FOnAttributeChangeData& Data) override;

	// 패턴 테이블에서 쿨타임·거리 조건을 통과한 패턴 중 가중치 랜덤으로 하나를 골라 그 어빌리티를 활성화한다.
	// 테이블이 없거나 고를 패턴이 없으면 공격하지 않는다.
	virtual bool ActivateAIAttack() override;

	// 지금 대상에게 쓸 수 있는(쿨타임이 끝났고 사용 거리 안인) 패턴 행과 그 가중치 합을 모은다.
	void GatherUsablePatterns(
		const AActor* Target,
		TArray<TPair<FName, const FBossPatternRow*>>& OutCandidates,
		float& OutTotalWeight
	) const;

	// 패턴 행이 지금 페이즈에서 쓸 수 있고(PhaseWeights > 0), 쿨타임이 아니고, 대상과의 거리가 패턴의 사용 거리 안인지.
	// bAssumeInAttackDistance면 실제 거리와 함께 AttackDistance에 있는 것으로도 검사한다.
	bool IsPatternUsable(
		FName RowName,
		const FBossPatternRow& Row,
		float DistanceToTarget,
		bool bAssumeInAttackDistance
	) const;

	// 패턴 행의 쿨타임이 남아 있는지. 쿨타임은 DT_BossPatterns 행의 Cooldown 값으로 패턴을 시작할 때부터 센다.
	bool IsPatternOnCooldown(FName RowName) const;

	/**
	 * 같은 방향으로 달아나는 대상을 쫓는 중인지.
	 * 대상이 보스 앞쪽 안쪽 감지 범위(발각 반경) 안에 있고, 보스 정면 방향으로 ChaseAssumeMinTargetSpeed 이상 달리고 있으면 true.
	 * 이때는 대상이 공격 거리 안에 들어온 것으로 보고 패턴을 고른다. 속도가 같아 거리를 좁히지 못하는 상황을 위한 처리다.
	 */
	bool IsChasingFleeingTarget(const AActor* Target) const;

	// 쓸 수 있는 패턴 중 가중치 랜덤으로 하나를 고른다. 고를 행이 없으면 nullptr. OutTotalWeight에 후보 가중치 합을 돌려준다.
	const FBossPatternRow* PickNextPattern(const AActor* Target, FName& OutRowName, float& OutTotalWeight) const;

	// 고른 패턴을 화면과 출력 로그에 남긴다.
	void LogPatternChoice(FName RowName, const FBossPatternRow& Row, float TotalWeight) const;

	// 패턴 어빌리티가 끝나거나 취소되면 PatternRestTimeMin~Max 사이의 휴식 시간 뒤에 다음 패턴을 시작하게 한다.
	void OnPatternAbilityEnded(const FAbilityEndedData& EndedData);

	// FaceDirection으로 정한 방향까지 TurnRate만큼 회전한다.
	void UpdateTurning(float DeltaTime);

	/** 좌우 방향 전환 속도(도/초). 0이면 즉시 돈다. 덩치 큰 보스가 순간적으로 뒤돌지 않도록 반 바퀴를 0.5초에 돈다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement", meta = (ClampMin = "0.0", Units = "deg/s"))
	float TurnRate = 360.0f;

	// 방향 전환 목표 Yaw와 진행 여부
	float TargetFacingYaw = 0.0f;
	bool bTurning = false;

	/** 공격 패턴 가중치 테이블 (행 구조체: FBossPatternRow). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern", meta = (RequiredAssetDataTags = "RowStructure=/Script/EclipseTheNinja.BossPatternRow"))
	TObjectPtr<UDataTable> PatternTable;

	/** 고른 패턴을 화면에 표시할지. 출력 로그에는 항상 남긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern")
	bool bPrintPatternOnScreen = true;

	/** 같은 방향으로 달아나는 대상이 안쪽 감지 범위 안에 있으면 공격 거리에 들어온 것으로 보고 패턴을 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern")
	bool bAssumeAttackRangeWhileChasing = true;

	/** 대상이 이 속도(cm/s) 이상으로 보스 정면 방향으로 달아나야 추격 중으로 본다. 제자리에서 돌아선 대상은 제외된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern", meta = (ClampMin = "0.0", Units = "cm/s", EditCondition = "bAssumeAttackRangeWhileChasing"))
	float ChaseAssumeMinTargetSpeed = 150.0f;

	/** 패턴이 끝난 뒤 다음 패턴까지 쉬는 최소 시간(초). 플레이어가 한 텀 쉬어갈 수 있는 틈이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern", meta = (ClampMin = "0.0", Units = "s"))
	float PatternRestTimeMin = 1.0f;

	/** 패턴이 끝난 뒤 다음 패턴까지 쉬는 최대 시간(초). 최소값과 같으면 항상 같은 시간만큼 쉰다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern", meta = (ClampMin = "0.0", Units = "s"))
	float PatternRestTimeMax = 2.0f;

	/** 휴식 중 대상과 유지할 좌우 거리(cm). 이보다 멀면 다가가고, 가까우면 제자리에서 대상을 바라본다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern", meta = (ClampMin = "0.0", Units = "cm"))
	float PatternRestKeepDistance = 200.0f;

	// 마지막으로 실행한 패턴 어빌리티. 실행 중 판정과 취소에 사용한다.
	FGameplayAbilitySpecHandle CurrentPatternHandle;

	// ASC의 어빌리티 종료 델리게이트 바인딩
	FDelegateHandle PatternEndedHandle;

	// 패턴 행 이름별로 다시 쓸 수 있게 되는 월드 시간(초).
	TMap<FName, double> PatternReadyTimes;

	// 실드를 켜고 제한시간 타이머를 시작한다.
	void ActivateEnergyLink(int32 LinkIndex);

	// 제한시간 안에 실드를 해제하지 못하면 플레이어 입력을 막고 처형을 시작한다.
	void OnEnergyLinkTimeExpired();

	// 플레이어가 땅에 서면(또는 ExecutionGroundWaitTime이 지나면) 처형 어빌리티를 실행한다. 아니면 잠시 뒤 다시 시도한다.
	void TryStartExecution();

	// 처형 어빌리티를 부여하고 실행한다. 실패하면 false.
	bool ActivateExecutionAbility();

	// 실드 상태와 타이머만 정리한다. 알림은 보내지 않는다.
	void ClearEnergyLinkState();

#if WITH_EDITOR
	// 디테일 패널의 디버그 값으로 피해·암살·레버를 흉내 낸다.
	void ProcessDebugRequests();
#endif

	/** 실드가 켜지는 체력 비율. 높은 값부터 순서대로 한 번씩 발동한다. 기획: 70% / 30%. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Energy Link")
	TArray<float> EnergyLinkThresholds = { 0.7f, 0.3f };

	/** 실드 해제(기믹) 제한시간(초). 지나면 보스가 플레이어를 처형한다. 0 이하면 시간 제한 없이 레버를 기다린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Energy Link", meta = (ClampMin = "0.0", Units = "s"))
	float EnergyLinkTimeLimit = 60.0f;

	/**
	 * 실드를 제한시간 안에 해제하지 못했을 때 실행할 처형 어빌리티. 기본은 UGABossExecution.
	 * 비우거나 실행에 실패하면 연출 없이 플레이어를 즉사시킨다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Execution")
	TSubclassOf<UGameplayAbility> ExecutionAbilityClass;

	/**
	 * 처형할 때 보스가 재생하는 암살자 몽타주. 보스 스켈레톤용이어야 한다.
	 * 일반 암살의 AM_Assassination_Attack은 닌자 스켈레톤이라 리타깃한 몽타주를 넣는다. 비우면 플레이어만 쓰러진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Execution")
	TSoftObjectPtr<UAnimMontage> ExecutionAttackMontage;

	/** 처형당하는 플레이어가 재생하는 몽타주. 기본은 일반 암살의 피격자 몽타주(AM_Assassination_Victim). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Execution")
	TSoftObjectPtr<UAnimMontage> ExecutionVictimMontage;

	/** 보스가 플레이어 등 뒤 몇 cm에 서서 처형할지. 두 캡슐이 겹치는 거리보다 가까워지지는 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Execution", meta = (ClampMin = "0.0", Units = "cm"))
	float ExecutionBehindDistance = 150.0f;

	/** 제한시간이 끝났을 때 플레이어가 공중이나 벽에 있으면 착지를 기다리는 최대 시간. 지나면 그 자리에서 처형한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Execution", meta = (ClampMin = "0.0", Units = "s"))
	float ExecutionGroundWaitTime = 1.5f;

	// 착지 대기를 시작한 월드 시간(초)과 재시도 타이머
	double ExecutionWaitStartTime = 0.0;
	FTimerHandle ExecutionWaitTimer;

	/** 이 체력 비율 이하에서 암살에 성공하면 보스를 즉시 처치한다. 기획: 20%. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Assassination", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AssassinationKillRatio = 0.2f;

	/** 실드가 켜져 있는 동안 보스 주위에 디버그 구를 그린다. 실드 이펙트가 생기기 전 테스트용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Debug")
	bool bDrawDebugShield = true;

	/**
	 * 공격 판정을 디버그로 그린다. BP에서는 SetDrawDebugAttack으로 켜고 끈다.
	 * - 근접 패턴: Boss Attack Hit 노티파이마다 판정 상자(대상이 안에 있으면 초록, 없으면 빨강)와 걸린 대상 위치
	 * - 검기 패턴: 발사 위치와 조준 방향, 날아가는 검기의 HitBox(빨강)와 이동용 Collision(노랑)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Debug")
	bool bDrawDebugAttack = false;

	/** 근접 판정·발사 방향 그림을 화면에 남겨 둘 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Debug", meta = (ClampMin = "0.0", Units = "s", EditCondition = "bDrawDebugAttack"))
	float DebugAttackDrawDuration = 1.0f;

#if WITH_EDITORONLY_DATA
	/** PIE 테스트용. 0보다 큰 값을 넣으면 다음 Tick에 플레이어가 준 피해처럼 적용하고 0으로 되돌린다. */
	UPROPERTY(EditInstanceOnly, Transient, Category = "Boss|Debug")
	float DebugDamage = 0.0f;

	/** PIE 테스트용. 체크하면 다음 Tick에 암살 성공을 받은 것처럼 처리하고 해제된다. */
	UPROPERTY(EditInstanceOnly, Transient, Category = "Boss|Debug")
	bool bDebugAssassinate = false;

	/** PIE 테스트용. 체크하면 다음 Tick에 레버를 당긴 것처럼 실드를 해제하고 해제된다. */
	UPROPERTY(EditInstanceOnly, Transient, Category = "Boss|Debug")
	bool bDebugBreakEnergyLink = false;

	/** PIE 테스트용. 실드가 켜져 있을 때 체크하면 다음 Tick에 제한시간이 끝난 것처럼 처형을 시작하고 해제된다. */
	UPROPERTY(EditInstanceOnly, Transient, Category = "Boss|Debug")
	bool bDebugExpireEnergyLink = false;
#endif

	/** 이 페이즈부터 ChaseSpeed로 달린다. 그 전 페이즈는 StanceWalkSpeed로 걷는다. 기획: 기믹 2회 파훼 후(3페이즈). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Phase", meta = (ClampMin = "1"))
	int32 RunPhase = 3;

	/**
	 * RunPhase 전 페이즈의 전투 이동 속도(cm/s).
	 * 자세 걷기 애니메이션은 1배속에 약 93cm/s를 걷고 재생 속도는 2배까지라, 이보다 빠르면 발이 미끄러진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Phase", meta = (ClampMin = "1.0", Units = "cm/s"))
	float StanceWalkSpeed = 180.0f;

	/** 파훼한 에너지 링크 수. 페이즈는 이 값 + 1이다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Phase")
	int32 BrokenEnergyLinkCount = 0;

	/** 다음에 발동할 EnergyLinkThresholds 순번. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Energy Link")
	int32 NextEnergyLinkIndex = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Energy Link")
	bool bEnergyLinkActive = false;

	FTimerHandle EnergyLinkTimer;
};
