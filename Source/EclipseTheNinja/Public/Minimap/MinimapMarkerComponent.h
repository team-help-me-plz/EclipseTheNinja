#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Minimap/MinimapMarkerTypes.h"
#include "MinimapMarkerComponent.generated.h"

/**
 * 액터 하나의 미니맵 마커를 바꾼다. 붙이지 않아도 프로젝트 설정의 클래스 규칙대로 표시된다.
 *  - bHideMarker: 규칙에 맞아도 표시하지 않는다
 *  - bOverrideStyle: 규칙 대신 Style로 그린다. 규칙에 없는 클래스도 이걸로 표시할 수 있다
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ECLIPSETHENINJA_API UMinimapMarkerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMinimapMarkerComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	bool bHideMarker = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	bool bOverrideStyle = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap", meta = (EditCondition = "bOverrideStyle"))
	FMinimapMarkerStyle Style;
};
