#pragma once

#include "CoreMinimal.h"
#include "Character/Enemy.h"
#include "AlphaBlend.h"
#include "Animation/AnimEnums.h"
#include "CeilingEnemy.generated.h"

class UAnimSequenceBase;
class UAnimMontage;

/** 천장 매복 적의 진행 단계 */
UENUM(BlueprintType)
enum class ECeilingEnemyPhase : uint8
{
	// 천장에 매달려 아래를 지켜본다. 중력과 BT가 꺼져 있다.
	Hanging,
	// 기습 애니메이션의 하강 궤적을 따라 내려오며 공격 중이다.
	Ambushing,
	// 기습 전에 맞아서 공격 없이 떨어지는 중이다.
	Dropping,
	// 착지한 뒤 일반 근접 적처럼 행동한다.
	Grounded
};

/**
 * 천장에 매달려 있다가 아래로 지나가는 플레이어를 기습하는 근접 적.
 *
 * 배치: 일반 적처럼 천장 아래 바닥에 세워 둔다. 시작할 때 위로 캡슐을 쓸어 올려 천장에 붙고,
 * 매달린 자세의 가장 높은 뼈가 천장에 닿도록 메시 높이를 자동으로 맞춘다. 순찰 기준점은 배치한 위치다.
 *
 * 흐름: Hanging(중력 X, 감지 UI 숨김)
 *   -> 아래 감지 구역에 대상이 보이면 Ambushing(기습 애니메이션의 몸 하강 궤적을 실제 바닥 높이에 맞춰 수직으로 따라 내려오고,
 *      애니메이션이 바닥에 닿는 순간 착지·타격)
 *   -> 기습 전에 맞으면 Dropping(공격 없이 낙하)
 *   -> 착지 후 Grounded(대상을 전투 대상으로 지정해 기본 근접 적처럼 추적/공격).
 *
 * 행동 순서는 BT_CeilingEnemy가 정한다. Is On Ceiling 분기에서 Wait For Ambush Target → Ceiling Ambush를 실행하고,
 * 착지하면 같은 Selector의 Run Behavior(BT_Enemy) 분기로 넘어가 기본 근접 적과 같은 트리를 쓴다.
 * 이 클래스는 단계 전환과 매달림·하강 연출, 착지 처리만 맡는다.
 * 기습은 공격 1회로 계산해 착지 후 AttackInterval 동안 기본 공격을 하지 않는다.
 */
UCLASS()
class ECLIPSETHENINJA_API ACeilingEnemy : public AEnemy
{
	GENERATED_BODY()

public:
	ACeilingEnemy();

	/** 현재 진행 단계 */
	UFUNCTION(BlueprintPure, Category = "AI|Ceiling")
	ECeilingEnemyPhase GetCeilingPhase() const { return Phase; }

	/** 기습 대상. 기습 중에만 유효하다. */
	UFUNCTION(BlueprintPure, Category = "AI|Ceiling")
	APawn* GetAmbushTarget() const { return AmbushTarget.Get(); }

	/** 기습 어빌리티가 재생할 몽타주. 몽타주 에셋이면 그대로, 시퀀스면 블렌드 설정을 넣은 동적 몽타주를 만든다. */
	UAnimMontage* GetAmbushMontage();

	/** 기습 몽타주를 시작할 때의 재생 속도. 착지 전 하강 구간 속도를 쓴다. */
	float GetAmbushPlayRate() const { return FMath::Max(0.01f, AmbushDescentPlayRate); }

	/** 기습 몽타주를 시작할 위치(초). 하강 구간을 찾았다면 그 직전 AmbushMaxWindupTime부터 시작한다. */
	float GetAmbushStartTime() const;

	// 착지 순간 기습 타격 이벤트를 보내거나, 공격 없이 떨어진 경우 일반 상태로 돌아간다.
	virtual void Landed(const FHitResult& Hit) override;

	// 체크포인트 리스폰 1단계: 기본 초기화 후 다시 천장에 매달린다.
	virtual void ResetToInitial_Implementation() override;

	/**
	 * 아래 감지 구역에서 보이는 가장 가까운 대상을 찾는다. BT의 Wait For Ambush Target이 호출한다.
	 * DebugDrawTime은 bDrawAmbushTriggerDebug가 켜져 있을 때 감지 구역을 그려 둘 시간(초)이다.
	 */
	APawn* FindAmbushTarget(float DebugDrawTime) const;

	/** 대상을 향해 기습을 시작한다. BT의 Ceiling Ambush가 매달린 상태에서 호출한다. */
	void StartAmbush(APawn* Target);

protected:
	// BT가 처음 단계를 읽기 전에 천장 단계로 시작하도록, 컨트롤러가 빙의하기 전에 단계를 정한다.
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// 매달린 상태에서 피해를 받으면 공격 없이 떨어진다.
	virtual void OnHealthChanged(const FOnAttributeChangeData& Data) override;

	// 천장에 매달린다. 천장에 붙고, 중력·감지를 끄고 매달림 루프를 재생한다.
	void EnterHanging();

	// 캡슐을 위로 쓸어 올려 천장 바로 아래에 붙인다. 천장을 찾으면 CeilingZ를 기록한다.
	void SnapToCeiling();

	// 매달린 자세의 가장 높은 뼈가 천장에 닿도록 메시 높이를 맞춘다. 포즈가 갱신된 뒤에 호출한다.
	void AlignHangPoseToCeiling();

	// 대상 쪽(DirectionX 부호)으로 AmbushTurnTime 동안 돌도록 회전량을 정한다. 반 바퀴면 카메라 쪽으로 돈다.
	void StartAmbushTurn(float DirectionX);

	// 하강 궤적을 따라 캡슐을 옮긴다. 궤적이 끝나거나 바닥에 닿으면 착지 처리로 넘긴다.
	void UpdateProfiledDescent();

	// 궤적 이동을 끝내고 바로 아래 바닥에 내려앉게 한다. 착지하면 Landed에서 타격한다.
	void ReleaseDescentToFalling();

	// 기습하지 않고 떨어진다. 착지 후 공격자를 전투 대상으로 삼는다.
	void StartDropWithoutAttack(AActor* DamageInstigator);

	// 착지 후 일반 근접 적으로 전환한다. 감지를 다시 켜고 전투 대상을 지정한다. BT는 단계 변화를 보고 다음 분기로 넘어간다.
	void FinishDescent();

	// 매달린 자세(메시 오프셋, 루프 몽타주)를 푼다. 낙하나 사망 시 호출한다.
	void ReleaseHangPose(float BlendOutTime);

	// 매달림 루프 몽타주를 재생한다.
	void PlayHangLoop();

	// 루프 구간 안을 사인 곡선으로 천천히 오가도록 재생 위치를 정한다.
	void UpdateHangLoop(float DeltaTime);

	// 매달림 루프 구간(초)을 계산한다.
	void GetHangLoopRange(float& OutStart, float& OutEnd) const;

	// 기습 애니메이션 루트 트랙에서 하강 구간과 높이 변화를 한 번 계산해 둔다.
	void EnsureAmbushProfile();

	// 미리 뽑아 둔 기습 애니메이션 샘플에서 Time초 값을 보간한다.
	float SampleAmbushCurve(const TArray<float>& Samples, float Time) const;

	// 기습 애니메이션 Time초에 캡슐 기준으로 몸통 뼈가 놓이는 액터 공간 오프셋. 메시 회전·스케일을 반영한다.
	FVector GetBodyOffsetActorSpace(float Time) const;

	// 기습 애니메이션 Time초에서 캡슐이 시작 위치보다 내려가 있어야 할 거리(cm).
	// 루트가 고정된 포즈에서 몸(골반)이 원본 애니메이션의 몸 궤적을 따르도록 계산한다.
	float GetDescentDropAt(float Time) const;

	// 매달림 메시 오프셋을 Alpha(1 = 매달림, 0 = 기본 위치)만큼 적용한다.
	void ApplyHangMeshOffset(float Alpha);

	// 기습 어빌리티가 실행 중인지
	bool IsAmbushAbilityActive() const;

	/** 시작할 때 천장에 매달려 있을지. 끄면 처음부터 일반 근접 적으로 행동한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Ceiling")
	bool bStartOnCeiling = true;

	/** 매달려 있는 동안 바라볼 월드 방향(Yaw, 도). 배치 회전값과 관계없이 이 방향으로 매달린다. 0이면 +X(오른쪽). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Ceiling")
	float HangYaw = 270.0f;

	/** 배치 위치에서 위로 천장을 찾을 최대 거리(cm). 이 안에 천장이 없으면 배치한 높이에 그대로 매달린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Ceiling", meta = (ClampMin = "0.0", Units = "cm"))
	float CeilingSearchDistance = 1000.0f;

	/** 감지 구역의 좌우 반폭(cm). 적 X 위치 ± 이 값 안에 있어야 기습한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Ceiling|Detection", meta = (ClampMin = "1.0", Units = "cm"))
	float AmbushTriggerHalfWidth = 150.0f;

	/** 감지 구역의 깊이(cm). 천장에 붙은 캡슐 중심에서 이 거리만큼 아래까지 본다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Ceiling|Detection", meta = (ClampMin = "1.0", Units = "cm"))
	float AmbushTriggerDepth = 800.0f;

	/** 감지 구역을 디버그로 그린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Ceiling|Detection")
	bool bDrawAmbushTriggerDebug = false;

	/** 기습 애니메이션에서 하강 직전 준비 동작을 최대 몇 초까지 남길지. 나머지 앞부분은 건너뛴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Ceiling|Ambush", meta = (ClampMin = "0.0", Units = "s"))
	float AmbushMaxWindupTime = 0.15f;

	/** 기습 애니메이션에서 하강 구간을 찾지 못했을 때만 쓰는 낙하 시작 속도(cm/s). 이후 중력이 더해진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Ceiling|Ambush", meta = (ClampMin = "0.0", Units = "cm/s"))
	float AmbushDropSpeed = 1200.0f;

	/** 낙하 후 착지하지 못해도 일반 상태로 돌아가는 시간(초). 바닥이 없는 곳에 배치된 경우를 대비한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Ceiling|Ambush", meta = (ClampMin = "0.1", Units = "s"))
	float DescentTimeout = 4.0f;

	/** 매달린 자세 원본 애니메이션 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ceiling|Animation")
	TObjectPtr<UAnimSequenceBase> HangAnimation;

	/** 매달림 루프 시작 프레임 (원본 애니메이션 기준) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ceiling|Animation", meta = (ClampMin = "0"))
	int32 HangLoopStartFrame = 28;

	/** 매달림 루프 끝 프레임 (원본 애니메이션 기준) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ceiling|Animation", meta = (ClampMin = "0"))
	int32 HangLoopEndFrame = 30;

	/** 루프 구간을 한 번 왕복하는 시간(초). 구간이 몇 프레임뿐이라 길게 잡아야 떨리지 않고 숨 쉬듯 움직인다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ceiling|Animation", meta = (ClampMin = "0.1", Units = "s"))
	float HangLoopPeriod = 2.5f;

	/** 매달림·기습 몽타주를 재생할 ABP 슬롯 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ceiling|Animation")
	FName MontageSlotName = TEXT("DefaultSlot");

	/**
	 * 자동 높이 맞춤 뒤에 추가로 더할 메시 위치(cm). 손발이 천장을 조금 파고들거나 떠 보일 때만 미세 조정한다.
	 * 천장을 찾지 못한 경우에는 이 값만 적용된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Ceiling|Animation", meta = (Units = "cm"))
	FVector HangMeshOffset = FVector::ZeroVector;

	/** 하강 궤적의 기준이 되는 몸통 뼈. 화면에 보이는 몸이 이 뼈의 원본 궤적대로 내려오도록 캡슐을 옮긴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ceiling|Animation")
	FName AmbushBodyBoneName = TEXT("pelvis");

	/** 기습 애니메이션. 시퀀스면 아래 블렌드 값으로 동적 몽타주를 만들고, 몽타주면 그대로 재생한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ceiling|Animation")
	TObjectPtr<UAnimSequenceBase> AmbushAnimation;

	/** 매달린 자세에서 기습 자세로 섞이는 시간(초). 메시 높이 보정은 이 블렌드의 실제 가중치를 따라 함께 풀린다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ceiling|Animation", meta = (ClampMin = "0.0", Units = "s"))
	float AmbushBlendInTime = 0.2f;

	/** 기습할 때 대상 쪽으로 반 바퀴 도는 데 걸리는 시간(초). 매달림 자세가 빠진 뒤 돌기 시작한다. 0이면 바로 돈다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ceiling|Animation", meta = (ClampMin = "0.0", Units = "s"))
	float AmbushTurnTime = 0.25f;

	/** 기습이 끝나고 기본 자세로 돌아가는 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ceiling|Animation", meta = (ClampMin = "0.0", Units = "s"))
	float AmbushBlendOutTime = 0.25f;

	/** 기습 애니메이션 블렌드 곡선 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ceiling|Animation")
	EAlphaBlendOption AmbushBlendOption = EAlphaBlendOption::HermiteCubic;

	/** 기습 애니메이션 최소 시작 위치(초). 하강 구간을 찾지 못했을 때는 이 위치부터 재생한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ceiling|Animation", meta = (ClampMin = "0.0", Units = "s"))
	float AmbushStartTime = 0.0f;

	/** 착지 전까지 기습 애니메이션 재생 속도. 낮출수록 천천히 찍으며 내려온다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ceiling|Animation", meta = (ClampMin = "0.01"))
	float AmbushDescentPlayRate = 0.5f;

	/** 착지한 뒤 남은 기습 애니메이션 재생 속도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ceiling|Animation", meta = (ClampMin = "0.01"))
	float AmbushPlayRate = 1.0f;

	/** 공격 없이 떨어질 때 매달림 자세를 푸는 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Ceiling|Animation", meta = (ClampMin = "0.0", Units = "s"))
	float DropBlendOutTime = 0.15f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Ceiling")
	ECeilingEnemyPhase Phase = ECeilingEnemyPhase::Grounded;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> AmbushTarget;

	// 착지 후 전투 대상으로 지정할 액터 (기습 대상 또는 공격자)
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> PendingCombatActor;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> HangMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> AmbushDynamicMontage;

	double DescentStartTime = 0.0;
	bool bLandedDuringAmbush = false;

	// 천장 정보 (SnapToCeiling에서 찾는다)
	bool bHasCeiling = false;
	float CeilingZ = 0.0f;

	// 매달린 자세 높이 맞춤
	bool bHangPoseAligned = false;
	int32 HangTicksSinceStart = 0;
	// 매달린 몸이 천장에 닿고 캡슐 바로 위에 오도록 자동으로 더한 메시 위치 (액터 공간)
	FVector AutoHangMeshOffset = FVector::ZeroVector;

	// 메시 오프셋 적용 비율 (1 = 매달림 오프셋, 0 = 기본 위치). 매달림 몽타주 가중치를 따른다.
	float HangMeshOffsetAlpha = 0.0f;

	// 매달림 루프 왕복에 쓰는 경과 시간
	float HangLoopElapsed = 0.0f;

	// 기습 애니메이션 루트 높이 샘플과 하강 구간 (초, cm)
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ProfiledAmbushAnimation;

	TArray<float> AmbushRootZSamples;

	// 몸통 뼈의 컴포넌트 공간 높이 (루트 포함, 원본 애니메이션의 실제 몸 높이)
	TArray<float> AmbushBodyZSamples;

	// 루트를 기준 포즈로 고정했을 때 몸통 뼈의 컴포넌트 공간 위치 (게임에서 실제로 보이는 포즈)
	TArray<FVector> AmbushLockedBodySamples;
	float AmbushSampleRate = 60.0f;
	float AmbushDescentBeginTime = 0.0f;
	float AmbushDescentEndTime = 0.0f;
	float AmbushRootTopZ = 0.0f;
	float AmbushRootDrop = 0.0f;
	bool bAmbushProfileValid = false;

	// 이번 기습의 하강 이동 정보
	FVector DescentStartLocation = FVector::ZeroVector;
	float DescentHeight = 0.0f;
	// 찍기 자세에서 몸통이 캡슐 축 위에 오도록 메시에 더하는 보정 (액터 공간, 기습 몽타주 가중치 반영)
	FVector AmbushBodyCorrection = FVector::ZeroVector;

	// 기습 중 아직 돌아야 하는 회전량(도). 부호가 도는 방향이다.
	float AmbushTurnRemaining = 0.0f;

	// 몸 궤적을 실제 높이에 맞추는 배율과 그 기준 시각
	float DescentBodyScale = 1.0f;
	float DescentAnimStartTime = 0.0f;
	bool bUsingProfiledDescent = false;
	bool bDescentReleased = false;

	// 기습 중 루트 모션을 무시하기 전의 ABP 설정
	TEnumAsByte<ERootMotionMode::Type> SavedRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
	bool bRootMotionOverridden = false;
};
