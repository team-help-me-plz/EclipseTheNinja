#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MinimapBoundsVolume.generated.h"

class UBoxComponent;
class UMinimapData;

/**
 * 미니맵이 다룰 범위와 격자 설정. 맵마다 하나만 배치한다.
 *  - 상자의 X·Z 범위가 미니맵 범위가 된다 (회전은 무시하고 축 정렬 경계를 쓴다)
 *  - 격자의 Y(플레이 평면)는 상자가 아니라 플레이어의 Y를 따른다
 *  - 태그 MinimapIgnore가 붙은 액터는 벽 판정과 시야 트레이스에서 빠진다
 */
UCLASS()
class ECLIPSETHENINJA_API AMinimapBoundsVolume : public AActor
{
	GENERATED_BODY()

public:
	AMinimapBoundsVolume();

	FBox GetWorldBounds() const;

	/**
	 * 지금 레벨로 미니맵 격자를 만들어 BakedData 에셋에 저장한다. MinimapPaintVolume이 적용된다.
	 * BakedData가 비어 있으면 /Game/Data/Minimap/MD_<레벨 이름> 에셋을 만든다. 에셋은 바로 저장되고, 레벨은 직접 저장해야 한다.
	 */
	UFUNCTION(CallInEditor, Category = "Minimap")
	void BakeMinimap();

	/** 구운 미니맵. 있으면 런타임이 충돌 검사 없이 이 격자를 쓴다. 레벨을 고쳤으면 다시 굽는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	TObjectPtr<UMinimapData> BakedData = nullptr;

	/** 격자 한 칸의 크기(uu). 작을수록 자세하지만 격자 생성이 무거워진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap", meta = (ClampMin = "25.0"))
	float CellSize = 100.f;

	/** 벽 판정 상자의 Y 절반 두께. 플레이 평면에 걸친 메시만 벽으로 잡도록 얇게 둔다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap", meta = (ClampMin = "1.0"))
	float PlaneHalfThickness = 15.f;

	/** 벽 판정과 시야 트레이스에 쓰는 채널. 트리거와 폰은 Visibility를 무시하므로 기본값이 맞다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	TEnumAsByte<ECollisionChannel> OcclusionChannel = ECC_Visibility;

	/** 밝혀진 빈 칸 색 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Color")
	FColor EmptyColor = FColor(30, 36, 52, 235);

	/** 밝혀진 벽 안쪽 칸 색 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Color")
	FColor SolidColor = FColor(72, 74, 82, 255);

	/** 밝혀진 벽 가장자리(빈 칸과 맞닿은 벽) 칸 색 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Color")
	FColor EdgeColor = FColor(205, 192, 160, 255);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Bounds = nullptr;
};
