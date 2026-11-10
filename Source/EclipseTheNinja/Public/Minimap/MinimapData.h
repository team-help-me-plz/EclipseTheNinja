#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MinimapData.generated.h"

/** 미니맵 칸 종류 */
UENUM()
enum class EMinimapCellKind : uint8
{
	Empty = 0,	// 빈 칸
	Solid = 1,	// 벽 안쪽
	Edge = 2,	// 빈 칸과 맞닿은 벽 (윤곽)
	Hidden = 3,	// 밝혀져도 그리지 않는 칸 (비밀 통로 등)
};

/**
 * 에디터에서 구운 미니맵 격자. 레벨마다 하나씩 만든다.
 *  - MinimapBoundsVolume의 "Bake Minimap" 버튼(또는 콘솔 Eclipse.Minimap.Bake)이 만든다
 *  - 런타임은 이 데이터가 있으면 충돌 검사 없이 그대로 쓴다
 *  - 직접 고치지 말고 레벨의 MinimapPaintVolume을 고친 뒤 다시 굽는다
 */
UCLASS(BlueprintType)
class ECLIPSETHENINJA_API UMinimapData : public UDataAsset
{
	GENERATED_BODY()

public:
	bool IsValidGrid() const
	{
		return GridWidth > 0 && GridHeight > 0 && CellKinds.Num() == GridWidth * GridHeight
			&& (CellTints.IsEmpty() || CellTints.Num() == CellKinds.Num());
	}

	UPROPERTY(VisibleAnywhere, Category = "Minimap")
	int32 GridWidth = 0;

	UPROPERTY(VisibleAnywhere, Category = "Minimap")
	int32 GridHeight = 0;

	UPROPERTY(VisibleAnywhere, Category = "Minimap")
	float CellSize = 100.f;

	/** 격자 왼쪽 아래 모서리의 월드 X */
	UPROPERTY(VisibleAnywhere, Category = "Minimap")
	float OriginX = 0.f;

	/** 격자 왼쪽 아래 모서리의 월드 Z */
	UPROPERTY(VisibleAnywhere, Category = "Minimap")
	float OriginZ = 0.f;

	/** 구울 때 쓴 플레이 평면 Y */
	UPROPERTY(VisibleAnywhere, Category = "Minimap")
	float PlaneY = 0.f;

	/** 칸별 EMinimapCellKind. 인덱스 = Z * GridWidth + X (Z는 아래에서 위) */
	UPROPERTY()
	TArray<uint8> CellKinds;

	/** 칸별 색 덮어쓰기. 알파 0이면 기본 색. 페인트 볼륨에 Color가 하나도 없으면 비어 있다. */
	UPROPERTY()
	TArray<FColor> CellTints;
};
