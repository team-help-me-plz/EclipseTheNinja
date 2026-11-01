#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossEnemyGimmickComponent.generated.h"

class ABossEnemy;
class UNiagaraComponent;
class UNiagaraSystem;

/** 에너지 링크 한 번을 푸는 기믹 설정. LinkSteps의 순번이 보스의 EnergyLinkThresholds 순번과 같다. */
USTRUCT(BlueprintType)
struct FBossGimmickLinkStep
{
	GENERATED_BODY()

	/** 이 순번의 링크를 푸는 기믹 ID. 같은 ID의 기믹 포탈(ABossGimmickPortal)과 짝을 이룬다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick")
	FName LinkId;

	/**
	 * 보스 스테이지에서 레이저가 이어질 대상.
	 * 보통 기믹 스테이지로 가는 입구 포탈이며, 같은 맵의 레버로 바로 풀게 하려면 그 레버를 넣는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick")
	TObjectPtr<AActor> BeamTarget = nullptr;
};

/**
 * 보스의 에너지 링크 기믹 연출과 해제 순서를 맡는다.
 * - 링크가 켜지면 보스에서 지금 순번의 BeamTarget까지 레이저(LaserSystem)를 잇는다.
 *   레이저 시작점은 보스를 따라다니고, 끝점(BeamEndParameter)은 대상 위치에 고정된다.
 * - 같은 LinkId의 기믹 포탈들에 링크 상태를 알려, 입구 포탈을 열고 기믹 스테이지의 레이저를 켜게 한다.
 * - 기믹 포탈(레버의 대상)이 RequestBreakLink로 해제를 요청하면, 지금 순번의 LinkId일 때만 보스의 링크를 해제한다.
 *
 * 보스의 링크 발동·해제·실패 델리게이트로 구동되고, 체크포인트 리셋은 보스가 ResetGimmick으로 알린다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ECLIPSETHENINJA_API UBossEnemyGimmickComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossEnemyGimmickComponent();

	/** LinkId 기믹이 지금 켜져 있는 링크의 기믹이면 보스의 링크를 해제한다. 해제했으면 true. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Gimmick")
	bool RequestBreakLink(FName LinkId);

	/** LinkId 기믹의 링크가 지금 켜져 있는지. */
	UFUNCTION(BlueprintPure, Category = "Boss|Gimmick")
	bool IsLinkActive(FName LinkId) const;

	/** 지금 켜져 있는 링크의 기믹 ID. 켜져 있지 않으면 NAME_None. */
	UFUNCTION(BlueprintPure, Category = "Boss|Gimmick")
	FName GetActiveLinkId() const;

	/** 레이저를 끄고 기믹 포탈들도 닫는다. 보스의 체크포인트 리셋에서 호출한다. */
	void ResetGimmick();

	/** 월드에서 LinkId 기믹을 가진 보스의 기믹 컴포넌트를 찾는다. 없으면 nullptr. */
	static UBossEnemyGimmickComponent* FindByLinkId(const UObject* WorldContextObject, FName LinkId);

	/** 기믹 레이저 기본 나이아가라 에셋. 기믹 포탈도 같은 레이저를 쓴다. */
	static const TCHAR* DefaultLaserSystemPath;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleEnergyLinkActivated(int32 LinkIndex);

	UFUNCTION()
	void HandleEnergyLinkEnded();

	// 레이저를 켜고 끝점을 대상 위치에 고정한다.
	void StartLaser(const AActor* Target);

	void StopLaser();

	// 같은 LinkId의 기믹 포탈들에 링크 상태를 알린다.
	void NotifyPortals(FName LinkId, bool bActive) const;

	/** 에너지 링크 순번별 기믹. 순번은 보스의 EnergyLinkThresholds(높은 체력 순)와 같다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Gimmick")
	TArray<FBossGimmickLinkStep> LinkSteps;

	/** 보스와 대상을 잇는 레이저. 기본은 NS_Laser_Twist_2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Gimmick|Laser")
	TSoftObjectPtr<UNiagaraSystem> LaserSystem;

	/** 레이저 끝점을 받는 나이아가라 사용자 파라미터. 월드 좌표를 넣는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Gimmick|Laser")
	FName BeamEndParameter = TEXT("User.Beam End");

	/** 레이저 시작점. 보스 루트(캡슐 중심) 기준 상대 위치(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Gimmick|Laser")
	FVector BeamStartOffset = FVector(0.0f, 0.0f, 40.0f);

	/** 레이저 끝점. 대상 액터 위치 기준 월드 오프셋(cm). 레버 손잡이나 포탈 중심 높이에 맞춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Gimmick|Laser")
	FVector BeamTargetOffset = FVector(0.0f, 0.0f, 100.0f);

private:
	TWeakObjectPtr<ABossEnemy> Boss;

	// 지금 켜져 있는 LinkSteps 순번. 꺼져 있으면 INDEX_NONE
	int32 ActiveStepIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> LaserComp = nullptr;
};
