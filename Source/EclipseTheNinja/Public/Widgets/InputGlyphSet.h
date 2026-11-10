#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputCoreTypes.h"
#include "InputGlyphSet.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EInputGlyphDevice : uint8
{
	KeyboardMouse,
	Xbox,
	PlayStation
};

UCLASS(BlueprintType)
class ECLIPSETHENINJA_API UInputGlyphSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Glyphs")
	TMap<FKey, TObjectPtr<UTexture2D>> Keyboard;

	UPROPERTY(EditAnywhere, Category = "Glyphs")
	TMap<FKey, TObjectPtr<UTexture2D>> Xbox;

	UPROPERTY(EditAnywhere, Category = "Glyphs")
	TMap<FKey, TObjectPtr<UTexture2D>> PlayStation;

	UPROPERTY(EditAnywhere, Category = "Glyphs")
	TObjectPtr<UTexture2D> Unknown;

	UTexture2D* FindTexture(const FKey& Key, EInputGlyphDevice Device) const;
};
