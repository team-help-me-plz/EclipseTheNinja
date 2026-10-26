#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NoiseSettings.generated.h"

class UDataTable;
class ANoiseRingActor;

/**
 * Project Settings > Game > Noise
 * 서브시스템은 에디터에서 값을 지정할 수 없으므로 소음 테이블과 링 액터 클래스를 여기서 지정한다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Noise"))
class ECLIPSETHENINJA_API UNoiseSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

public:
	/** FNoiseTableRow 기반의 DT_Noise */
	UPROPERTY(Config, EditAnywhere, Category = "Noise", meta = (RequiredAssetDataTags = "RowStructure=/Script/EclipseTheNinja.NoiseTableRow"))
	TSoftObjectPtr<UDataTable>	NoiseTable;

	/** 소음마다 풀에서 꺼내 표시할 링 액터 (위젯 클래스가 지정된 블루프린트) */
	UPROPERTY(Config, EditAnywhere, Category = "Noise")
	TSoftClassPtr<ANoiseRingActor>	NoiseRingClass;
};
