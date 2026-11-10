#include "Subsystem/InputGlyphSubsystem.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "Widgets/SViewport.h"

class FInputGlyphProcessor final : public IInputProcessor
{
public:
	explicit FInputGlyphProcessor(UInputGlyphSubsystem& InOwner) : Owner(&InOwner) {}
	virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
	{
		if (Owner.IsValid()) Owner->HandleInput(Event.GetKey(), 1.f, Event.GetUserIndex(), Event.GetInputDeviceId());
		return false;
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication&, const FAnalogInputEvent& Event) override
	{
		if (Owner.IsValid()) Owner->HandleInput(Event.GetKey(), Event.GetAnalogValue(), Event.GetUserIndex(), Event.GetInputDeviceId());
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& Event) override
	{
		if (Owner.IsValid()) Owner->HandleInput(Event.GetEffectingButton(), 1.f, Event.GetUserIndex(), Event.GetInputDeviceId());
		return false;
	}

	virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent& Event) override
	{
		// 미세한 포인터 흔들림은 게임패드 표시를 바꾸지 않는다.
		if (Event.GetCursorDelta().SizeSquared() < 4.f) return false;
		if (Owner.IsValid()) Owner->HandleInput(EKeys::MouseX, 1.f, Event.GetUserIndex(), Event.GetInputDeviceId());
		return false;
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication&, const FPointerEvent& Event, const FPointerEvent*) override
	{
		if (Owner.IsValid()) Owner->HandleInput(EKeys::MouseWheelAxis, Event.GetWheelDelta(), Event.GetUserIndex(), Event.GetInputDeviceId());
		return false;
	}

private:
	TWeakObjectPtr<UInputGlyphSubsystem> Owner;
};

void UInputGlyphSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!FSlateApplication::IsInitialized()) return;

	InputProcessor = MakeShared<FInputGlyphProcessor>(*this);
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
}

void UInputGlyphSubsystem::Deinitialize()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();
	OnDeviceChanged.Clear();
	Super::Deinitialize();
}

void UInputGlyphSubsystem::HandleInput(const FKey& Key, float Amount, int32 UserIndex, FInputDeviceId DeviceId)
{
	const ULocalPlayer* Player = GetLocalPlayer();
	if (!Player) return;
	if (UserIndex != Player->GetPlatformUserIndex()) return;

	const UWorld* World = GetWorld();
	if (!World) return;
	if (!World->IsGameWorld()) return;

	const UGameViewportClient* ViewportClient = Player->ViewportClient;
	if (!ViewportClient) return;

	const TSharedPtr<SViewport> Viewport = ViewportClient->GetGameViewportWidget();
	if (!Viewport) return;
	if (!Viewport->HasKeyboardFocus() && !Viewport->HasFocusedDescendants()) return;

	// 아날로그 스틱의 데드존 안에서는 장치를 전환하지 않는다.
	if (FMath::Abs(Amount) < 0.25f) return;

	EInputGlyphDevice NewDevice = EInputGlyphDevice::KeyboardMouse;
	if (Key.IsGamepadKey())
	{
		NewDevice = EInputGlyphDevice::Xbox;
		if (const UInputDeviceSubsystem* Devices = UInputDeviceSubsystem::Get())
		{
			const FString Hardware = Devices->GetInputDeviceHardwareIdentifier(DeviceId).HardwareDeviceIdentifier.ToString();
			if (Hardware.Contains(TEXT("Dual")) || Hardware.Contains(TEXT("PlayStation")) || Hardware.Contains(TEXT("PS4")) || Hardware.Contains(TEXT("PS5")))
			{
				NewDevice = EInputGlyphDevice::PlayStation;
			}
		}
	}
	if (Device == NewDevice) return;

	Device = NewDevice;
	OnDeviceChanged.Broadcast();
}
