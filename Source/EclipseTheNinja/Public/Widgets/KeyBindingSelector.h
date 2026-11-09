#pragma once

#include "Components/InputKeySelector.h"
#include "KeyBindingSelector.generated.h"

class UKeyBindingSelector;
DECLARE_DELEGATE_TwoParams(FOnBindingKeyChanged, UKeyBindingSelector&, const FKey&);

UCLASS()
class UKeyBindingSelector : public UInputKeySelector
{
	GENERATED_BODY()

public:
	void SetDisplayedKey(const FKey& Key);

	FName MappingName;
	FText ActionLabel;
	FOnBindingKeyChanged OnBindingKeyChanged;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION()
	void HandleBindingSelected(FInputChord Chord);

	bool bUpdatingDisplayedKey = false;
};
