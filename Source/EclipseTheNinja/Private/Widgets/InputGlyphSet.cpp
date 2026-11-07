#include "Widgets/InputGlyphSet.h"

UTexture2D* UInputGlyphSet::FindTexture(const FKey& Key, EInputGlyphDevice Device) const
{
	const TMap<FKey, TObjectPtr<UTexture2D>>* Textures = &Keyboard;
	if (Device == EInputGlyphDevice::Xbox) Textures = &Xbox;
	if (Device == EInputGlyphDevice::PlayStation) Textures = &PlayStation;

	const TObjectPtr<UTexture2D>* Texture = Textures->Find(Key);
	if (Texture && *Texture) return *Texture;

	return Unknown;
}
