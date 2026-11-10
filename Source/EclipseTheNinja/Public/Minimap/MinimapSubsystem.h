#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Minimap/MinimapMarkerTypes.h"
#include "MinimapSubsystem.generated.h"

class AMinimapBoundsVolume;
class ATargetCutawayVolume;
class UTexture2D;

/**
 * 미니맵 격자와 텍스처를 관리한다.
 *  - 격자는 XZ 평면을 CellSize 칸으로 나눈 것이다. 칸마다 벽인지, 밝혀졌는지를 가진다
 *  - 플레이어가 처음 시야를 밝힐 때 플레이어의 Y를 플레이 평면으로 삼아 격자를 만든다
 *  - 시야는 플레이어 눈 위치에서 360도로 쏜 레이가 막히기 전까지 지나간 칸만 밝힌다
 *  - TargetCutawayVolume의 InteriorArea는 플레이어가 한 번 들어가기 전까지 레이를 막는 마스크다
 *  - 텍스처는 한 칸이 한 픽셀이며, 위젯이 그대로 그린다 (위쪽 행이 높은 Z)
 */
UCLASS()
class ECLIPSETHENINJA_API UMinimapSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UMinimapSubsystem* Get(const UObject* WorldContext);

	virtual void Deinitialize() override;

	// 에디터 월드에서는 만들지 않는다
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	void RegisterBounds(AMinimapBoundsVolume* InBounds);
	void UnregisterBounds(AMinimapBoundsVolume* InBounds);

	/** 컷어웨이 볼륨을 마스크 영역으로 등록한다. bDiscovered면 처음부터 레이를 막지 않는다. */
	void RegisterMaskRegion(ATargetCutawayVolume* InVolume, bool bDiscovered);
	void UnregisterMaskRegion(ATargetCutawayVolume* InVolume);

	/** 플레이어가 컷어웨이 볼륨에 들어갔다. 이후로 그 영역은 일반 시야 규칙만 따른다. */
	void DiscoverMaskRegion(ATargetCutawayVolume* InVolume);

	/** Eye에서 360도로 레이를 쏴 보이는 칸을 밝힌다. 격자가 없으면 Eye의 Y로 먼저 만든다. */
	void RevealSight(const FVector& Eye, float Radius, int32 RayCount, const AActor* Viewer);

	/** RevealSight와 같지만 레이를 원이 아니라 화면에 보이는 사각형(XZ, Eye 중심 반폭·반높이)까지만 쏜다. */
	void RevealSightInView(const FVector& Eye, const FVector2D& ViewHalfExtent, int32 RayCount, const AActor* Viewer);

	bool IsGridReady() const { return bGridReady; }
	FIntPoint GetGridSize() const { return FIntPoint(GridWidth, GridHeight); }
	float GetCellSize() const { return CellSize; }
	FColor GetSolidColor() const { return SolidColor; }
	FColor GetEdgeColor() const { return EdgeColor; }
	UTexture2D* GetTexture() const { return Texture; }

	/** 월드 위치를 텍스처 픽셀 좌표(왼쪽 위가 0,0, 소수 포함)로 바꾼다. */
	FVector2D WorldToTexturePixel(const FVector& WorldLocation) const;

	/** 발견한(마커 칸이 한 번이라도 밝혀진) 마커를 현재 위치·상태로 돌려준다. */
	void GetMarkerViews(TArray<FMinimapMarkerView>& OutViews) const;

	/** 마지막 기록 이후 밝힌 칸을 지금 진행 데이터에 남긴다. 스테이지 클리어 저장 직전(진행 맵이 바뀌기 전)에 부른다. */
	void FlushReveal();

private:
	struct FMarkerEntry
	{
		TWeakObjectPtr<AActor> Actor;
		FMinimapMarkerStyle Style;
		TWeakObjectPtr<UTexture2D> Icon;
		const FBoolProperty* StateProperty = nullptr;
		const FArrayProperty* PathProperty = nullptr;
		// 등록할 때(문이 닫혀 있을 때)의 충돌 범위. ActorBounds 모양에 쓴다
		FBox Bounds = FBox(ForceInit);
		bool bDiscovered = false;
	};

	// 레벨의 액터 중 마커 규칙(또는 MinimapMarkerComponent)에 맞는 것을 모은다
	void CollectMarkers();
	// 칸이 밝혀진 마커를 발견 처리한다
	void UpdateMarkerDiscovery();
	bool IsLocationVisibleOnMap(const FVector& WorldLocation) const;
	// 범위(XZ)에 걸친 칸 중 하나라도 밝혀졌으면 true
	bool IsBoxVisibleOnMap(const FBox& WorldBox) const;

	TArray<FMarkerEntry> Markers;

	// 마커 아이콘이 GC되지 않게 잡아 둔다
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> LoadedIcons;

	struct FMaskRegion
	{
		TWeakObjectPtr<ATargetCutawayVolume> Volume;
		FTransform BoxTransform;
		FVector BoxExtent = FVector::ZeroVector;
		bool bDiscovered = false;
	};

	// ViewHalfExtent가 있으면 그 사각형, 없으면 Radius 원까지 쏜다
	void RevealSightInternal(const FVector& Eye, float Radius, const FVector2D* ViewHalfExtent, int32 RayCount, const AActor* Viewer);

	bool BuildGrid(float PlaneY);
	void BuildMaskIndex(int32 RegionIndex);
	void RebuildAllMaskIndices();
	int32 FindRegion(const ATargetCutawayVolume* InVolume) const;
	void CollectIgnoredActors(TArray<const AActor*>& OutActors) const;
	static bool ShouldSkipActor(const AActor* InActor);

	bool WorldToCell(const FVector& WorldLocation, int32& OutX, int32& OutZ) const;
	int32 CellIndex(int32 X, int32 Z) const { return Z * GridWidth + X; }
	FVector CellCenter(int32 X, int32 Z) const;

	// 칸이 아직 발견하지 않은 마스크 영역 안이면 true
	bool IsCellMasked(int32 Index) const;

	// 밝혔으면 true (이미 밝혀져 있었으면 false)
	bool RevealCell(int32 Index);
	void WritePixel(int32 Index);
	void FlushTexture();

	int32 ComputeGridHash() const;
	void LoadSavedReveal();
	void RecordReveal();

	TWeakObjectPtr<AMinimapBoundsVolume> BoundsVolume;
	TArray<FMaskRegion> MaskRegions;

	bool bGridReady = false;
	int32 GridWidth = 0;
	int32 GridHeight = 0;
	float CellSize = 100.f;
	float OriginX = 0.f;
	float OriginZ = 0.f;
	float PlaneY = 0.f;
	TEnumAsByte<ECollisionChannel> OcclusionChannel = ECC_Visibility;
	FColor EmptyColor;
	FColor SolidColor;
	FColor EdgeColor;

	// 칸별 EMinimapCellKind
	TArray<uint8> CellKinds;
	// 칸별 색 덮어쓰기 (알파 0이면 기본 색). 비어 있을 수 있다
	TArray<FColor> CellTints;
	TArray<uint8> Revealed;
	// 칸이 속한 마스크 영역 인덱스, 없으면 INDEX_NONE
	TArray<int32> CellMaskIndex;
	TArray<FColor> Pixels;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Texture = nullptr;

	bool bTextureDirty = false;
	bool bSaveDirty = false;
	double LastRecordTime = 0.0;
};
