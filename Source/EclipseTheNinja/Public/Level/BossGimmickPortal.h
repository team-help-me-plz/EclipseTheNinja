#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/Activatable.h"
#include "BossGimmickPortal.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class ABossGimmickPortal;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGimmickPortalArrived, ABossGimmickPortal*, Portal, APawn*, Player);

/** 기믹 포탈이 놓인 곳. 같은 LinkId의 반대쪽 포탈로 이동한다. */
UENUM(BlueprintType)
enum class EBossGimmickPortalSide : uint8
{
	/** 보스 스테이지의 입구. 자기 링크가 켜져 있을 때만 열린다. */
	BossStage,
	/** 기믹 스테이지 시작점의 복귀 포탈. 항상 열려 있고, 링크가 켜져 있으면 스테이지 레버까지 레이저를 잇는다. */
	GimmickStage,
};

/**
 * 보스 기믹 스테이지를 오가는 포탈.
 * - 플레이어가 트리거에 닿으면 같은 LinkId의 반대쪽 포탈 ArrivalPoint로 순간이동한다.
 *   기믹 스테이지는 보스 맵에 레벨 인스턴스로 들어가므로 보스 상태와 링크 제한시간이 그대로 이어진다.
 * - 보스 스테이지 포탈은 보스의 기믹 순번이 자기 LinkId일 때만 열려, 레버를 정해진 순서로만 풀 수 있다.
 * - 기믹 스테이지 포탈은 레버(StageLever)의 TargetActors에 넣는다. 레버를 당기면 보스에게 링크 해제를 요청한다.
 * - 링크 상태는 보스의 UBossEnemyGimmickComponent가 SetLinkActive로 알려 준다.
 */
UCLASS()
class ECLIPSETHENINJA_API ABossGimmickPortal : public AActor, public IActivatable
{
	GENERATED_BODY()

public:
	ABossGimmickPortal();

	/** 짝을 이루는 기믹 ID. */
	FName GetLinkId() const { return LinkId; }

	/** 포탈이 놓인 곳. */
	EBossGimmickPortalSide GetPortalSide() const { return PortalSide; }

	/** 보스의 링크 상태를 반영한다. 보스 스테이지 포탈은 열리고 닫히며, 기믹 스테이지 포탈은 레이저를 켜고 끈다. */
	void SetLinkActive(bool bActive);

	/** 지금 플레이어를 들여보내는지. */
	UFUNCTION(BlueprintPure, Category = "Gimmick Portal")
	bool IsPortalOpen() const;

	/** 순간이동한 플레이어가 설 위치. 트리거 밖에 두어야 도착하자마자 되돌아가지 않는다. */
	FTransform GetArrivalTransform() const;

	/** 도착 직후 잠시 동안 이 포탈로 들어가지 못하게 한다. */
	void BlockEntry(float Duration);

	/** 반대쪽 포탈에서 순간이동한 플레이어가 이 포탈에 도착하면(화면이 다시 밝아지기 시작할 때) 호출된다. */
	UPROPERTY(BlueprintAssignable, Category = "Gimmick Portal")
	FOnGimmickPortalArrived OnPlayerArrived;

	// 스테이지 레버가 당겨지면 보스에게 링크 해제를 요청한다.
	virtual void ToggleActivate_Implementation() override;

	virtual void ShowActivationIndicator_Implementation() override {}
	virtual void HideActivationIndicator_Implementation() override {}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	// 같은 LinkId의 반대쪽 포탈을 찾는다. 기믹 스테이지 레벨이 아직 로드되지 않았으면 nullptr.
	ABossGimmickPortal* FindDestination() const;

	// 화면을 어둡게 한 뒤 플레이어를 반대쪽 포탈로 옮긴다.
	void BeginTeleport(APawn* Player);

	// 화면이 어두워지면 실제로 옮기고 다시 밝힌다.
	void FinishTeleport();

	// 열림 상태에 맞춰 포탈 이펙트를 켜고 끈다.
	void RefreshPortalVisual();

	// 스테이지 레버까지 레이저를 켜거나 끈다.
	void RefreshStageLaser();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Trigger = nullptr;

	/** 순간이동해 온 플레이어가 서는 위치와 방향. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> ArrivalPoint = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> PortalVFX = nullptr;

	/** 짝을 이루는 기믹 ID. 보스 기믹 컴포넌트의 LinkSteps와 반대쪽 포탈에 같은 값을 넣는다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick Portal")
	FName LinkId;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick Portal")
	EBossGimmickPortalSide PortalSide = EBossGimmickPortalSide::BossStage;

	/** 기믹 스테이지 포탈만 사용. 링크를 푸는 레버이며, 링크가 켜져 있는 동안 포탈에서 이 레버까지 레이저를 잇는다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick Portal", meta = (EditCondition = "PortalSide == EBossGimmickPortalSide::GimmickStage"))
	TObjectPtr<AActor> StageLever = nullptr;

	/**
	 * 기믹 스테이지 포탈만 사용. 레버로 링크를 해제하면 함께 작동(IActivatable::ToggleActivate)시킬 액터들.
	 * 복귀 지름길의 문이나 발판을 넣어, 레버를 당긴 뒤 시작점으로 돌아가는 길을 연다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick Portal", meta = (EditCondition = "PortalSide == EBossGimmickPortalSide::GimmickStage"))
	TArray<TObjectPtr<AActor>> ActivateOnLinkBroken;

	/** 화면이 어두워지는(그리고 다시 밝아지는) 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick Portal", meta = (ClampMin = "0.0", Units = "s"))
	float FadeTime = 0.3f;

	/** 도착한 포탈에 다시 들어가지 못하는 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick Portal", meta = (ClampMin = "0.0", Units = "s"))
	float ArrivalBlockTime = 1.0f;

	/** 포탈이 열려 있을 때 재생하는 이펙트. 기본은 NS_SpatialDistortion7. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick Portal|VFX")
	TSoftObjectPtr<UNiagaraSystem> PortalSystem;

	/** 스테이지 레버까지 잇는 레이저. 기본은 보스와 같은 NS_Laser_Twist_2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick Portal|VFX")
	TSoftObjectPtr<UNiagaraSystem> LaserSystem;

	/** 레이저 끝점을 받는 나이아가라 사용자 파라미터. 월드 좌표를 넣는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick Portal|VFX")
	FName BeamEndParameter = TEXT("User.Beam End");

	/** 레이저 시작점. 포탈 기준 상대 위치(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick Portal|VFX")
	FVector BeamStartOffset = FVector(0.0f, 0.0f, 100.0f);

	/** 레이저 끝점. 레버 위치 기준 월드 오프셋(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick Portal|VFX")
	FVector BeamTargetOffset = FVector(0.0f, 0.0f, 100.0f);

private:
	// 보스의 링크가 지금 이 포탈의 LinkId인지
	bool bLinkActive = false;

	// 이 시간(월드 초)까지는 들어가지 못한다
	double EntryBlockedUntil = 0.0;

	// 화면이 어두워지는 동안 옮길 플레이어와 목적지
	TWeakObjectPtr<APawn> TeleportingPlayer;
	TWeakObjectPtr<ABossGimmickPortal> TeleportDestination;

	FTimerHandle TeleportTimer;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> LaserComp = nullptr;
};
