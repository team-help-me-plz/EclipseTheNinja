#include "Widgets/InputGlyphWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Subsystem/InputGlyphSubsystem.h"
#include "UserSettings/EnhancedInputUserSettings.h"

bool UInputGlyphWidget::Initialize()
{
	if (!Super::Initialize()) return false;
	if (!WidgetTree) return false;

	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
	Scale->SetStretch(EStretch::ScaleToFit);
	WidgetTree->RootWidget = Scale;
	GlyphImage = WidgetTree->ConstructWidget<UImage>();
	Scale->AddChild(GlyphImage);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

void UInputGlyphWidget::NativeConstruct()
{
	Super::NativeConstruct();
	LoadedGlyphSet = GlyphSet.LoadSynchronous();
	EnhancedInput = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetOwningLocalPlayer());
	if (EnhancedInput.IsValid())
	{
		EnhancedInput->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &UInputGlyphWidget::RefreshGlyph);
		InputSettings = EnhancedInput->GetUserSettings();
	}
	if (InputSettings.IsValid())
	{
		InputSettings->OnSettingsChanged.AddUniqueDynamic(this, &UInputGlyphWidget::HandleSettingsChanged);
	}

	DeviceSubsystem = ULocalPlayer::GetSubsystem<UInputGlyphSubsystem>(GetOwningLocalPlayer());
	if (DeviceSubsystem.IsValid())
	{
		DeviceChangedHandle = DeviceSubsystem->OnDeviceChanged.AddUObject(this, &UInputGlyphWidget::RefreshGlyph);
	}
	RefreshGlyph();
}

void UInputGlyphWidget::NativeDestruct()
{
	if (EnhancedInput.IsValid()) EnhancedInput->ControlMappingsRebuiltDelegate.RemoveDynamic(this, &UInputGlyphWidget::RefreshGlyph);
	if (InputSettings.IsValid()) InputSettings->OnSettingsChanged.RemoveDynamic(this, &UInputGlyphWidget::HandleSettingsChanged);
	if (DeviceSubsystem.IsValid()) DeviceSubsystem->OnDeviceChanged.Remove(DeviceChangedHandle);
	DeviceChangedHandle.Reset();
	EnhancedInput.Reset();
	InputSettings.Reset();
	DeviceSubsystem.Reset();
	Super::NativeDestruct();
}

void UInputGlyphWidget::SetInputAction(const UInputAction* Action)
{
	InputAction = Action;
	RefreshGlyph();
}

void UInputGlyphWidget::HandleSettingsChanged(UEnhancedInputUserSettings*)
{
	RefreshGlyph();
}

FKey UInputGlyphWidget::ResolveKey() const
{
	if (!InputAction) return FKey();
	if (!EnhancedInput.IsValid()) return FKey();

	bool bGamepad = false;
	if (DeviceSubsystem.IsValid() && DeviceSubsystem->GetDevice() != EInputGlyphDevice::KeyboardMouse) bGamepad = true;

	// 등록된 기본 매핑은 조준 등으로 IMC가 잠시 빠져도 계속 표시한다.
	if (InputSettings.IsValid())
	{
		const UEnhancedPlayerMappableKeyProfile* Profile = InputSettings->GetActiveKeyProfile();
		TArray<const UInputMappingContext*> Contexts;
		for (const UInputMappingContext* Context : InputSettings->GetRegisteredInputMappingContexts()) Contexts.Add(Context);
		Contexts.Sort([](const UInputMappingContext& A, const UInputMappingContext& B) { return A.GetPathName() < B.GetPathName(); });
		for (const UInputMappingContext* Context : Contexts)
		{
			for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
			{
				if (Mapping.Action != InputAction) continue;
				if (Mapping.Key.IsGamepadKey() != bGamepad) continue;

				TArray<FKey> Keys;
				if (Profile) Profile->GetPlayerMappedKeysForRebuildControlMappings(Mapping, Keys);
				if (!Keys.IsEmpty()) return Keys[0];
				return Mapping.Key;
			}
		}
	}

	for (const FKey& Key : EnhancedInput->QueryKeysMappedToAction(InputAction))
	{
		if (Key.IsGamepadKey() == bGamepad) return Key;
	}
	return FKey();
}

void UInputGlyphWidget::RefreshGlyph()
{
	if (!GlyphImage) return;
	if (!LoadedGlyphSet) return;

	DisplayedKey = ResolveKey();
	EInputGlyphDevice Device = EInputGlyphDevice::KeyboardMouse;
	if (DeviceSubsystem.IsValid()) Device = DeviceSubsystem->GetDevice();

	UTexture2D* Texture = LoadedGlyphSet->FindTexture(DisplayedKey, Device);
	GlyphImage->SetBrushFromTexture(Texture, true);
	SetToolTipText(DisplayedKey.GetDisplayName());
}
