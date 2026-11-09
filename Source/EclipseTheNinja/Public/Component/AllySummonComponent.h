#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AllySummonComponent.generated.h"

class AEnemy;
class UAnimMontage;
class UObjectPoolDataAsset;
struct FEnemyClimbEntrance;

/**
 * 전투에 들어가면 한 번 동료를 불러, 배경 쪽 가장자리 너머에서 기어올라오게 하는 소환 기능.
 *
 * 흐름: 전투 단계에 들어가고 아직 부르지 않았으면 WantsToSummon이 true가 된다.
 *   -> BT의 Wants Summon Allies 분기가 Summon Allies 태스크로 GA_SummonAllies를 실행한다.
 *   -> GA가 SummonMontage를 재생하고, "Summon Allies" 노티파이 시점에 SummonAllies를 호출한다.
 *   -> 소환된 적은 오브젝트 풀에서 꺼내 체크포인트 런타임 액터로 등록하고, UEnemyClimbEntranceComponent로 등장시킨다.
 * 소환 위치는 주인 옆 X 지점마다 플레이 평면의 바닥에서 카메라 반대쪽으로 쓸어 가며 바닥이 끊기거나 벽이 막는 가장자리를 찾는다.
 * 체크포인트 리스폰 때 ResetSummon으로 소환 기회를 되돌린다. 소환된 적은 체크포인트가 풀에 반환한다.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class ECLIPSETHENINJA_API UAllySummonComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAllySummonComponent();

	/** 지금 소환을 시작해야 하는지. 전투 중이고 아직 부르지 않았거나, 소환 동작이 진행 중이면 true다. */
	UFUNCTION(BlueprintPure, Category = "AI|Summon")
	bool WantsToSummon() const;

	/** 이번 생애에 이미 동료를 불렀는지 */
	UFUNCTION(BlueprintPure, Category = "AI|Summon")
	bool HasSummoned() const { return bHasSummoned; }

	/** 소환 동작(몽타주)이 진행 중인지 */
	UFUNCTION(BlueprintPure, Category = "AI|Summon")
	bool IsSummonInProgress() const { return bSummonInProgress; }

	// 소환 동작을 시작하거나 끝냈음을 기록한다. GA가 호출한다.
	void SetSummonInProgress(bool bInProgress) { bSummonInProgress = bInProgress; }

	// 소환 대상(지금 노리는 플레이어 또는 분신)
	APawn* GetSummonTarget() const;

	// 소리쳐 지원을 부르는 몽타주
	UAnimMontage* GetSummonMontage() const { return SummonMontage; }

	// 소환 몽타주 재생 속도. 0 이하가 되지 않게 막는다.
	float GetSummonPlayRate() const { return FMath::Max(0.01f, SummonPlayRate); }

	// 동료를 SummonCount만큼 불러 등장시킨다. 한 생애에 한 번만 실행되며 실제로 꺼낸 수를 돌려준다.
	int32 SummonAllies(AActor* CombatActor);

	// 체크포인트 리스폰용. 소환 기회를 되돌린다.
	void ResetSummon();

protected:
	virtual void BeginPlay() override;

	// 월드 X 위치의 등장 경로를 찾는다. 그 X의 플레이 평면에 바닥이 없으면 false다.
	bool FindEntrance(float X, float BackgroundDirectionY, FEnemyClimbEntrance& OutEntrance) const;
	bool IsEntranceClear(const FEnemyClimbEntrance& Entrance) const;

	// 카메라 반대쪽(배경 쪽) Y 방향. 카메라를 찾지 못하면 +Y다.
	float GetBackgroundDirectionY() const;

	// 소환한 적 하나를 풀에서 꺼내 등장시킨다.
	AEnemy* SpawnAlly(const FEnemyClimbEntrance& InEntrance, AActor* CombatActor, float Delay) const;

	/** 소환할 적 클래스. UEnemyClimbEntranceComponent를 붙여 두고, AutoPossessAI가 Spawned를 포함해야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Summon")
	TSubclassOf<AEnemy> SummonClass;

	/**
	 * 소환할 적의 풀 설정. 프로젝트 설정에 없는 풀이면 시작할 때 등록하고 SummonCount만큼 미리 만든다.
	 * 비우면 프로젝트 설정의 풀을 쓰고, 그것도 없으면 일반 스폰·파괴로 처리된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Summon")
	TObjectPtr<UObjectPoolDataAsset> SummonPoolData;

	/** 한 번에 부를 수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Summon", meta = (ClampMin = "0"))
	int32 SummonCount = 2;

	/**
	 * 주인에게서 대상 쪽으로 떨어진 X 거리(cm). 한 마리마다 하나씩 쓰고, 수가 더 많으면 마지막 간격으로 이어 붙인다.
	 * 그 자리에 바닥이 없으면 반대쪽 같은 거리에서 다시 찾는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Summon")
	TArray<float> SummonOffsetsX = { 150.0f, 280.0f };

	/** 동료끼리 기어오르기 시작 시간 간격(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Summon", meta = (ClampMin = "0.0", Units = "s"))
	float SummonInterval = 0.3f;

	/** 소리쳐 지원을 부르는 몽타주. "Summon Allies" 노티파이 시점에 소환한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Summon|Animation")
	TObjectPtr<UAnimMontage> SummonMontage;

	/** 소환 몽타주 재생 속도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Summon|Animation", meta = (ClampMin = "0.01"))
	float SummonPlayRate = 1.0f;

	/** 플레이 평면에서 배경 쪽 가장자리를 찾을 최대 거리(cm). 유효한 가장자리가 없으면 소환하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Summon|Location", meta = (ClampMin = "10.0", Units = "cm"))
	float MaxEdgeSearchDistance = 800.0f;

	/** 가장자리를 찾을 때 한 번에 나아가는 거리(cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Summon|Location", meta = (ClampMin = "5.0", Units = "cm"))
	float EdgeSearchStep = 25.0f;

	/** 바닥 위 이 높이(cm)에서 배경 쪽을 막는 벽을 찾는다. 이보다 낮은 턱은 바닥으로 본다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Summon|Location", meta = (ClampMin = "1.0", Units = "cm"))
	float EdgeWallProbeHeight = 40.0f;

	/** 플레이 평면 바닥보다 이만큼(cm) 낮아지면 바닥이 끊긴 것으로 본다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Summon|Location", meta = (ClampMin = "1.0", Units = "cm"))
	float EdgeFloorTolerance = 30.0f;

	/** 기어오르기를 시작할 깊이(cm). 가장자리 바닥보다 이만큼 아래에서 올라온다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Summon|Location", meta = (ClampMin = "0.0", Units = "cm"))
	float ClimbStartDepth = 180.0f;

	/** 기어오르기를 시작할 위치가 가장자리 너머로 떨어진 거리(cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Summon|Location", meta = (ClampMin = "0.0", Units = "cm"))
	float ClimbStartBehindEdge = 40.0f;

	/** 가장자리 위에 올라설 때 가장자리에서 안쪽으로 들어오는 여유(cm). 캡슐 반지름에 더한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Summon|Location", meta = (ClampMin = "0.0", Units = "cm"))
	float LedgeInset = 10.0f;

	/** 찾은 등장 경로를 디버그로 그린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Summon|Location")
	bool bDrawSummonDebug = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<AEnemy> OwnerEnemy;

	bool bHasSummoned = false;
	bool bSummonInProgress = false;
};
