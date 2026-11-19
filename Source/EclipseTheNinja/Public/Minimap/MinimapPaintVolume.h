#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MinimapPaintVolume.generated.h"

class UBoxComponent;

UENUM(BlueprintType)
enum class EMinimapPaintMode : uint8
{
	/** 충돌이 없어도 벽으로 그린다 (장식 벽 등) */
	Solid,
	/** 충돌이 있어도 빈 칸으로 그린다 (처음엔 닫혀 있는 문, 부서지는 발판 자리 등) */
	Empty,
	/** 밝혀져도 그리지 않는다 (비밀 통로, 환풍구 내부 등) */
	Hidden,
	/** 칸 색만 바꾼다 (위험 구역 등) */
	Color,
};

/**
 * 미니맵을 구울 때 상자 안의 칸을 원하는 대로 바꾼다. 에디터 전용 액터라 패키징된 게임에는 들어가지 않는다.
 *  - 칸 중심이 상자 안에 있으면 적용된다. 회전된 상자도 된다
 *  - 겹치면 Priority가 큰 볼륨이 나중에 적용되어 이긴다
 *  - Solid/Empty를 적용한 뒤 윤곽을 계산하므로 칠한 벽에도 윤곽이 생긴다
 *  - 바꾼 뒤에는 MinimapBoundsVolume의 Bake Minimap을 다시 눌러야 반영된다
 */
UCLASS()
class ECLIPSETHENINJA_API AMinimapPaintVolume : public AActor
{
	GENERATED_BODY()

public:
	AMinimapPaintVolume();

	virtual void OnConstruction(const FTransform& Transform) override;

	UBoxComponent* GetBox() const { return Box; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	EMinimapPaintMode Mode = EMinimapPaintMode::Empty;

	/** Color 모드에서 쓸 색 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap", meta = (EditCondition = "Mode == EMinimapPaintMode::Color"))
	FColor Color = FColor(170, 50, 45, 255);

	/** 겹칠 때 큰 값이 이긴다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	int32 Priority = 0;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Box = nullptr;
};
