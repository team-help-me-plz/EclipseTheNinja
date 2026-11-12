#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyFloorVolume.generated.h"

class UBoxComponent;

/**
 * 층 하나의 범위를 정하는 볼륨. 플레이어가 있는 층과 그 인접 층의 적만 활동하게 한다.
 * 충돌을 쓰지 않고 위치가 박스 안에 있는지만 계산하므로 다른 판정(바닥 검사, 오버랩)에 영향을 주지 않는다.
 */
UCLASS()
class ECLIPSETHENINJA_API AEnemyFloorVolume : public AActor
{
	GENERATED_BODY()

public:
	AEnemyFloorVolume();

	// 월드 위치가 이 볼륨의 박스 안에 있는지 확인한다.
	bool ContainsLocation(const FVector& Location) const;

	// 이 볼륨의 층 번호
	int32 GetFloorIndex() const { return FloorIndex; }

	// 박스 부피. 볼륨이 겹친 곳에서 더 작은 쪽을 고를 때 쓴다.
	double GetBoxVolume() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor")
	TObjectPtr<UBoxComponent> FloorBox;

	/** 층 번호. 위아래로 붙은 층은 1 차이가 나게 매긴다. 활성 범위는 이 번호 차이로 판단한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor")
	int32 FloorIndex = 0;
};
