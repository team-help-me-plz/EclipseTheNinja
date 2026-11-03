#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemySpacingComponent.generated.h"

class AEnemy;

/**
 * 추격·공격 중 적의 자리 잡기를 맡는다. AEnemy만 소유한다.
 * - 간격 유지: 앞에 있는 몬스터와 MonsterSpacingGap만큼 떨어져 선다.
 * - 포위: 한쪽에 몰리면 맨 뒤 몬스터부터 대상 반대편으로 건너가 양쪽에서 감싼다.
 * - 대상 간격: 대상 캡슐과 TargetSpacingGap만큼 떨어져 서고, 겹치면 대상을 바라본 채 물러난다.
 * - 낭떠러지 검사: 가려는 쪽 발밑에 바닥이 없으면 멈춘다.
 * - 공격 우선: 대상 근처에서 공격 쿨타임이 끝나면 몬스터 간격을 무시하고 공격하러 간다.
 *
 * 스스로 Tick하지 않고 AEnemy의 MoveAlongX·UpdateCombatMovement와 공격 BT 태스크가 호출한다.
 * 이웃 몬스터는 BeginPlay에 등록된 컴포넌트 목록에서 찾는다.
 * AEnemy가 생성자에서만 만든다. 다른 액터에 붙이지 못하도록 BlueprintSpawnableComponent를 두지 않는다.
 */
UCLASS(ClassGroup=(Custom))
class ECLIPSETHENINJA_API UEnemySpacingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemySpacingComponent();

	// 추격 중 다른 몬스터와 간격을 유지하는지.
	bool IsKeepingMonsterSpacing() const { return bKeepMonsterSpacing; }

	// 간격 유지를 켜거나 끈다. 혼자 움직이는 보스가 생성자에서 끈다.
	void SetKeepMonsterSpacing(bool bEnable) { bKeepMonsterSpacing = bEnable; }

	// 낭떠러지 쪽으로 걸어가지 않는지.
	bool IsAvoidingLedges() const { return bAvoidLedges; }

	// 대상 반대편으로 건너가는 중인지. 건너가는 동안은 간격 유지와 공격 시작을 하지 않는다.
	bool IsCrossingSurroundSide() const { return bCrossingSurroundSide; }

	// 간격 때문에 멈춰 있는지.
	bool IsHeldBySpacing() const { return bHeldBySpacing; }

	// 마지막 추격 목적지 계산에서 몬스터 간격보다 공격하러 가는 것을 우선하기로 했는지.
	bool IsPrioritizingAttack() const { return bPrioritizingAttack; }

	// 간격 때문에 멈춘 상태를 푼다. 간격을 보지 않는 이동에서 호출한다.
	void ClearSpacingHold() { bHeldBySpacing = false; }

	// 정해 둔 포위 자리를 비운다. 추격·공격이 끝날 때 호출하며 다음 추격에서 다시 정한다.
	void ResetSurround();

	// 앞쪽 몬스터와의 간격을 반영해 이동 목적지를 당긴다. 간격 때문에 멈춰야 하면 현재 X를 돌려준다.
	float ApplyMonsterSpacing(float DestinationX);

	// 다른 몬스터와 캡슐이 겹쳐 있으면 양보하는 쪽이 비켜 선다. 비켜 서는 중이면 true.
	// FaceTarget이 있으면 대상에서 먼 쪽이 양보하고, 대상을 바라본 채 대상 반대쪽으로 물러난다.
	bool TrySeparateFromMonsters(float Speed, float DeltaTime, const AActor* FaceTarget = nullptr);

	// 공격 거리 안에서 대상이나 다른 몬스터와 겹쳐 있으면 대상을 바라본 채 뒤로 물러난다. 몬스터끼리는 대상에서 먼 쪽이 물러난다. 물러나는 중이면 true.
	bool SeparateFromMonstersInAttackRange(const AActor* Target, float DeltaTime);

	// 대상 간격과 포위 자리를 반영한 추격 목적지 X. 포위 자리와 건너가는 중인지를 함께 갱신한다.
	float UpdateCombatDestinationX(const AActor* Target);

	// DirectionX 쪽 바로 앞이 낭떠러지인지. bAvoidLedges가 꺼져 있거나 공중이면 false.
	bool IsLedgeAhead(float DirectionX) const;

	// 이 X 위치 발밑에 딛을 바닥이 있는지.
	bool HasGroundAtX(float X) const;

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 추격 중 다른 몬스터와 간격을 유지할지. 혼자 움직이는 보스는 끈다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Spacing")
	bool bKeepMonsterSpacing = true;

	// 앞에 있는 몬스터와 캡슐 사이에 남겨 둘 간격. 음수면 그만큼 캡슐이 겹친 채 선다. 두 반경 합만큼 음수면 같은 자리까지 붙는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Spacing", meta = (ClampMin = "-200", Units = "cm", EditCondition = "bKeepMonsterSpacing"))
	float MonsterSpacingGap = 40.0f;

	// 간격 때문에 멈춘 뒤 이 거리 이상 벌어져야 다시 걷는다. 경계에서 섰다 걸었다를 반복하지 않게 한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Spacing", meta = (ClampMin = "0", Units = "cm", EditCondition = "bKeepMonsterSpacing"))
	float MonsterSpacingResumeDistance = 40.0f;

	// 낭떠러지 쪽으로 걸어가지 않고 가장자리 앞에서 멈출지. 끄면 걸어서 떨어질 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
	bool bAvoidLedges = true;

	// 대상(플레이어·분신)과 캡슐 사이에 남겨 둘 간격. 음수면 그만큼 겹친 채 선다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Spacing", meta = (ClampMin = "-200", Units = "cm", EditCondition = "bKeepMonsterSpacing"))
	float TargetSpacingGap = 10.0f;

	// 추격 중 동료가 적은 쪽으로 대상을 지나 건너가서 양쪽에서 감싸는지. 끄면 한쪽으로만 줄을 선다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Spacing", meta = (EditCondition = "bKeepMonsterSpacing"))
	bool bSurroundTarget = true;

	// 반대편으로 건너갈 때 대상 X를 지나 이만큼 더 가서 선다. AttackDistance보다 작아야 건너간 뒤 바로 공격한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Spacing", meta = (ClampMin = "0", Units = "cm", EditCondition = "bKeepMonsterSpacing && bSurroundTarget"))
	float SurroundCrossDistance = 60.0f;

	// 공격 쿨타임이 끝났을 때 대상이 이 X 거리 안이면 몬스터 간격을 무시하고 공격하러 간다. 멀리서 추격할 때는 줄을 선다. 0이면 끈다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Spacing", meta = (ClampMin = "0", Units = "cm", EditCondition = "bKeepMonsterSpacing"))
	float AttackPriorityDistance = 300.0f;

private:
	// 공격 쿨타임이 끝났고 대상이 AttackPriorityDistance 안이라 몬스터 간격보다 공격을 우선해야 하는지.
	bool ShouldPrioritizeAttack(const AActor* Target) const;

	// 등록된 다른 컴포넌트의 소유 적이 간격 유지에서 고려할 이웃이면 돌려준다. 아니면 nullptr.
	const AEnemy* GetSpacingNeighbor(const UEnemySpacingComponent* Other) const;

	// 상대 몬스터와 유지할 중심 사이 거리. 두 캡슐 반경 + MonsterSpacingGap이며 0 아래로 내려가지 않는다.
	float GetMonsterSpacingDistance(const AEnemy* Other) const;

	// 대상과 유지할 중심 사이 거리. 내 캡슐 반경 + 대상 반경 + TargetSpacingGap이며 0 아래로 내려가지 않는다.
	float GetTargetSpacingDistance(const AActor* Target) const;

	// 멈춰 있는 대상과 많이 겹쳐 있으면 대상을 바라본 채 간격을 되찾을 때까지 물러난다. 물러나는 중이면 true.
	bool TryBackAwayFromTarget(const AActor* Target, float DeltaTime);

	// 양쪽에 선 동료 수를 보고 내가 설 쪽을 정한다. 한쪽이 많으면 그쪽 맨 뒤 몬스터부터 반대편으로 보낸다.
	float ChooseSurroundSide(const AActor* Target) const;

	// 포위 인원을 셀 때 내가 속한 쪽. 건너가는 중이면 가려는 쪽, 아니면 지금 서 있는 쪽이다.
	float GetSurroundHomeSide(float TargetX) const;

	// 이 컴포넌트를 가진 적. OnRegister에서 잡는다.
	UPROPERTY(Transient)
	TObjectPtr<AEnemy> OwnerEnemy;

	// 간격 때문에 멈춰 있는지. 다시 걷기 시작할 거리 판정에 쓴다.
	bool bHeldBySpacing = false;

	// 대상에게서 물러나는 중인지. 간격을 다 되찾을 때까지 유지해 한 발짝씩 끊어 물러나지 않게 한다.
	bool bBackingAwayFromTarget = false;

	// 대상의 어느 쪽에 서기로 했는지(+1 / -1). 추격·공격 중이 아니면 0이다.
	float SurroundSide = 0.0f;

	// 대상 반대편으로 건너가는 중인지.
	bool bCrossingSurroundSide = false;

	// 몬스터 간격보다 공격하러 가는 것을 우선하는 중인지. UpdateCombatDestinationX가 매번 갱신한다.
	bool bPrioritizingAttack = false;
};
