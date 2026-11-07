#include "Widgets/KeyBindingSelector.h"

TSharedRef<SWidget> UKeyBindingSelector::RebuildWidget()
{
	OnKeySelected.AddUniqueDynamic(this, &UKeyBindingSelector::HandleBindingSelected);
	return Super::RebuildWidget();
}

void UKeyBindingSelector::SetDisplayedKey(const FKey& Key)
{
	// Slate는 표시값을 갱신할 때도 선택 이벤트를 전달한다.
	TGuardValue<bool> UpdateGuard(bUpdatingDisplayedKey, true);
	SetSelectedKey(FInputChord(Key));
}

void UKeyBindingSelector::HandleBindingSelected(FInputChord Chord)
{
	if (bUpdatingDisplayedKey) return;

	OnBindingKeyChanged.ExecuteIfBound(*this, Chord.Key);
}
