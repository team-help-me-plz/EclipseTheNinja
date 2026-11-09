#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Widgets/InputGlyphSet.h"
#include "InputGlyphSubsystem.generated.h"

class IInputProcessor;

DECLARE_MULTICAST_DELEGATE(FInputGlyphDeviceChanged);

UCLASS()
class ECLIPSETHENINJA_API UInputGlyphSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void HandleInput(const FKey& Key, float Amount, int32 UserIndex, FInputDeviceId DeviceId);
	EInputGlyphDevice GetDevice() const { return Device; }
	FInputGlyphDeviceChanged OnDeviceChanged;

private:
	TSharedPtr<IInputProcessor> InputProcessor;
	EInputGlyphDevice Device = EInputGlyphDevice::KeyboardMouse;
};
