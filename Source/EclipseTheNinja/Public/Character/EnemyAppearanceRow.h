#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EnemyAppearanceRow.generated.h"

/** 적 종류별로 공유하는 외형 색상. 메시의 세부 무늬는 머티리얼에 유지한다. */
USTRUCT(BlueprintType)
struct ECLIPSETHENINJA_API FEnemyAppearanceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor Color = FLinearColor::White;
};
