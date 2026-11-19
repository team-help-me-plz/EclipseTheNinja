#pragma once

#include "CoreMinimal.h"

class AActor;
class UPrimitiveComponent;
class UWorld;
class AMinimapBoundsVolume;

/** 격자 생성 결과. UMinimapData와 같은 내용이다. */
struct FMinimapGridResult
{
	int32 Width = 0;
	int32 Height = 0;
	float CellSize = 100.f;
	float OriginX = 0.f;
	float OriginZ = 0.f;
	float PlaneY = 0.f;
	TArray<uint8> Kinds;	// EMinimapCellKind
	TArray<FColor> Tints;	// 비어 있거나 Kinds와 같은 길이
};

/**
 * 미니맵 격자를 만든다. 에디터 굽기와 런타임(구운 데이터가 없을 때)이 같은 코드를 쓴다.
 *  1. 칸마다 플레이 평면에 얇은 상자를 놓고 막는 충돌이 있으면 벽
 *  2. MinimapPaintVolume 적용 (Priority 오름차순)
 *  3. 빈 칸과 맞닿은 벽을 윤곽으로
 *  4. Hidden 칠한 칸을 Hidden으로
 */
namespace MinimapGrid
{
	/** 이 태그가 붙은 액터는 벽 판정과 시야 트레이스에서 뺀다 */
	extern ECLIPSETHENINJA_API const FName IgnoreTag;

	/** 폰, MinimapIgnore 태그 액터처럼 지형이 아닌 액터면 true */
	ECLIPSETHENINJA_API bool ShouldSkipActor(const AActor* InActor);

	/**
	 * 컴포넌트 단위로 지형이 아닌 것을 거른다.
	 *  - 상자·구·캡슐 같은 모양 컴포넌트 (레버 상호작용 상자처럼 Visibility를 막는 트리거)
	 *  - 덩굴(BurnableVine)의 메시. 에디터에서는 막히지만 게임에서는 BeginPlay에서 충돌을 끈다
	 *  - 움직이는 발판·부서지는 발판·낙하물 (시작 위치가 땅으로 굳지 않게, 마커로만 표시)
	 *  - ShouldSkipActor에 걸리는 액터의 컴포넌트
	 */
	ECLIPSETHENINJA_API bool ShouldSkipComponent(const UPrimitiveComponent* InComponent);

	/** 지도 벽 판정용. ShouldSkipComponent에 더해 문(BaseDoor)도 뺀다. 시야 트레이스에서는 닫힌 문이 계속 막는다. */
	ECLIPSETHENINJA_API bool ShouldSkipForGrid(const UPrimitiveComponent* InComponent);

	/** 컷어웨이 볼륨의 대상 액터(외벽)와 그 자식 액터. 마스크가 대신 막으므로 벽 판정·트레이스에서 뺀다 */
	ECLIPSETHENINJA_API void CollectCutawayTargets(UWorld* World, TArray<const AActor*>& OutActors);

	ECLIPSETHENINJA_API bool Build(UWorld* World, const AMinimapBoundsVolume* Bounds, float PlaneY, FMinimapGridResult& Out);
}
