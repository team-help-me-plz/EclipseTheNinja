#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Minimap/MinimapMarkerTypes.h"
#include "MinimapMarkerSettings.generated.h"

/**
 * 프로젝트 설정 > Game > Eclipse Minimap Markers.
 * 레벨에 배치된 액터 중 Rules의 클래스에 맞는 것을 미니맵 마커로 자동 표시한다.
 * 액터 하나만 바꾸거나 숨기려면 그 액터에 MinimapMarkerComponent를 붙인다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Eclipse Minimap Markers"))
class ECLIPSETHENINJA_API UMinimapMarkerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMinimapMarkerSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** 위에서부터 처음 맞는 규칙을 쓴다. 자식 클래스(레버)를 부모 클래스(스위치)보다 위에 둔다. */
	UPROPERTY(Config, EditAnywhere, Category = "Markers")
	TArray<FMinimapMarkerRule> Rules;

	/** 흐리게 그릴 때의 불투명도 */
	UPROPERTY(Config, EditAnywhere, Category = "Markers", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DimmedOpacity = 0.35f;
};
