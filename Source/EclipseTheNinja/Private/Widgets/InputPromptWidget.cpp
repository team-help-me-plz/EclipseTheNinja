#include "Widgets/InputPromptWidget.h"
#include "Brushes/SlateColorBrush.h"
#include "Character/PlayerCharacterController.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/RichTextBlockImageDecorator.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Texture2D.h"
#include "Subsystem/InputGlyphSubsystem.h"
#include "UserSettings/EnhancedInputUserSettings.h"

void UInputPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshLabel();
}

void UInputPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshLabel();
}

void UInputPromptWidget::SetPromptAction(UInputAction* Action, const FText& KeyboardFallback)
{
	PromptAction = Action;
	KeyboardFallbackText = KeyboardFallback;
	RefreshLabel();
}

void UInputPromptWidget::SetQTEPromptAction(UInputAction* Action, const FText& KeyboardFallback)
{
	LoadedGlyphSet = GlyphSet.LoadSynchronous();
	SetPromptAction(Action, KeyboardFallback);
}

FText UInputPromptWidget::FormatKey(const FKey& Key)
{
	static const TMap<FKey, FString> Labels = {
		{EKeys::Gamepad_FaceButton_Bottom, TEXT("A")}, {EKeys::Gamepad_FaceButton_Right, TEXT("B")},
		{EKeys::Gamepad_FaceButton_Left, TEXT("X")}, {EKeys::Gamepad_FaceButton_Top, TEXT("Y")},
		{EKeys::Gamepad_LeftShoulder, TEXT("LB")}, {EKeys::Gamepad_RightShoulder, TEXT("RB")},
		{EKeys::Gamepad_LeftTrigger, TEXT("LT")}, {EKeys::Gamepad_RightTrigger, TEXT("RT")},
		{EKeys::Gamepad_Left2D, TEXT("왼쪽 스틱")}, {EKeys::Gamepad_Right2D, TEXT("오른쪽 스틱")},
		{EKeys::Gamepad_Special_Left, TEXT("View")}, {EKeys::Gamepad_Special_Right, TEXT("Menu")},
		{EKeys::Gamepad_DPad_Up, TEXT("D-Pad ↑")}, {EKeys::Gamepad_DPad_Down, TEXT("D-Pad ↓")},
		{EKeys::Gamepad_DPad_Left, TEXT("D-Pad ←")}, {EKeys::Gamepad_DPad_Right, TEXT("D-Pad →")},
		{EKeys::LeftMouseButton, TEXT("좌클릭")}, {EKeys::RightMouseButton, TEXT("우클릭")},
		{EKeys::SpaceBar, TEXT("Space")}, {EKeys::LeftControl, TEXT("Ctrl")}, {EKeys::LeftShift, TEXT("Shift")}
	};
	if (const FString* Label = Labels.Find(Key)) return FText::FromString(*Label);
	return Key.GetDisplayName();
}

FText UInputPromptWidget::GetPromptLabel(bool bGamepad) const
{
	return ResolveActionLabel(this, GetOwningPlayer(), PromptAction, FallbackMappingContext, KeyboardFallbackText, bGamepad);
}

FName UInputPromptWidget::GetIconNameForLabel(const FText& Label)
{
	static const TMap<FString, FName> Names = {
		{TEXT("A"),TEXT("A")},{TEXT("B"),TEXT("B")},{TEXT("X"),TEXT("X")},{TEXT("Y"),TEXT("Y")},
		{TEXT("LB"),TEXT("LB")},{TEXT("RB"),TEXT("RB")},{TEXT("LT"),TEXT("LT")},{TEXT("RT"),TEXT("RT")},
		{TEXT("Menu"),TEXT("Menu")},{TEXT("View"),TEXT("View")},
		{TEXT("왼쪽 스틱"),TEXT("LeftStick")},{TEXT("오른쪽 스틱"),TEXT("RightStick")},
		{TEXT("D-Pad ↑"),TEXT("DPad_Up")},{TEXT("D-Pad ↓"),TEXT("DPad_Down")},
		{TEXT("D-Pad ←"),TEXT("DPad_Left")},{TEXT("D-Pad →"),TEXT("DPad_Right")}
	};
	if (const FName* Name = Names.Find(Label.ToString())) return *Name;
	return NAME_None;
}

FText UInputPromptWidget::ResolveActionLabel(const UObject* WorldContext, APlayerController* Controller,
	const UInputAction* Action, const UInputMappingContext* MappingContext, const FText& KeyboardFallback, bool bGamepad)
{
	const FKey Key = ResolveActionKey(WorldContext, Controller, Action, MappingContext, KeyboardFallback, bGamepad);
	if (Key.IsValid()) return FormatKey(Key);
	return bGamepad ? FText::GetEmpty() : KeyboardFallback;
}

FKey UInputPromptWidget::ResolveActionKey(const UObject* WorldContext, APlayerController* Controller,
	const UInputAction* Action, const UInputMappingContext* MappingContext, const FText& KeyboardFallback, bool bGamepad)
{
	if (!Action) return FKey();
	TArray<FKey> Keys;
	APlayerController* PC = Controller;
	if (!PC && WorldContext && WorldContext->GetWorld()) PC = UGameplayStatics::GetPlayerController(WorldContext, 0);
	const UEnhancedInputLocalPlayerSubsystem* Subsystem = nullptr;
	if (PC && PC->GetLocalPlayer())
	{
		Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
		if (Subsystem) Keys = Subsystem->QueryKeysMappedToAction(Action);
	}
	const bool bHasDeviceKey = Keys.ContainsByPredicate([bGamepad](const FKey& Key) { return Key.IsGamepadKey() == bGamepad; });
	if (!bHasDeviceKey)
	{
		// 분신 조준·은신처럼 기본 IMC가 잠시 빠져 있으면 적용된 키가 없다.
		// IMC 에셋의 기본 키 대신, 사용자 키 설정(Key Profile)에서 바꾼 키를 먼저 쓴다.
		const UEnhancedInputUserSettings* Settings = Subsystem ? Subsystem->GetUserSettings() : nullptr;
		const UEnhancedPlayerMappableKeyProfile* Profile = Settings ? Settings->GetActiveKeyProfile() : nullptr;
		TArray<const UInputMappingContext*> Contexts;
		if (MappingContext) Contexts.Add(MappingContext);
		if (Settings)
		{
			TArray<const UInputMappingContext*> Registered;
			for (const UInputMappingContext* Context : Settings->GetRegisteredInputMappingContexts()) Registered.Add(Context);
			Registered.Sort([](const UInputMappingContext& A, const UInputMappingContext& B) { return A.GetPathName() < B.GetPathName(); });
			for (const UInputMappingContext* Context : Registered) Contexts.AddUnique(Context);
		}
		for (const UInputMappingContext* Context : Contexts)
		{
			if (!Context) continue;
			for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
			{
				if (Mapping.Action != Action) continue;
				TArray<FKey> MappedKeys;
				if (Profile) Profile->GetPlayerMappedKeysForRebuildControlMappings(Mapping, MappedKeys);
				if (MappedKeys.IsEmpty()) MappedKeys.Add(Mapping.Key);
				for (const FKey& Key : MappedKeys) Keys.AddUnique(Key);
			}
		}
	}
	// Keep an existing E/R/mouse caption when it is still one of the real bindings.
	if (!bGamepad && !KeyboardFallback.IsEmpty())
		for (const FKey& Key : Keys)
			if (!Key.IsGamepadKey() && FormatKey(Key).EqualTo(KeyboardFallback)) return Key;
	for (const FKey& Key : Keys)
		if (Key.IsGamepadKey() == bGamepad) return Key;
	return FKey();
}

void UInputPromptWidget::RefreshLabel()
{
	// QTE can set its action before NativeConstruct. Cache the UMG design width
	// before that first update, rather than shrinking the background to zero or
	// later treating an expanded caption width as the original size.
	USizeBox* Box = Cast<USizeBox>(GetRootWidget());
	if (Box && OriginalWidth <= 0.0f)
	{
		// Read the designer template as the source of truth, including for an
		// already-open prompt whose runtime width was changed before this fix.
		if (UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass()))
		{
			if (const UWidgetBlueprintGeneratedClass* OwnerClass = WidgetClass->FindWidgetTreeOwningClass())
			{
				if (const UWidgetTree* TemplateTree = OwnerClass->GetWidgetTreeArchetype())
					if (const USizeBox* TemplateBox = Cast<USizeBox>(TemplateTree->RootWidget))
						OriginalWidth = TemplateBox->GetWidthOverride();
			}
		}
		if (OriginalWidth <= 0.0f) OriginalWidth = Box->GetWidthOverride();
	}
	if (!KeyText) return;
	APlayerController* PC = GetOwningPlayer();
	if (!PC && GetWorld()) PC = UGameplayStatics::GetPlayerController(this, 0);
	const APlayerCharacterController* CharacterController = Cast<APlayerCharacterController>(PC);
	bool bGamepad = CharacterController && CharacterController->IsUsingGamepad();
	EInputGlyphDevice Device = bGamepad ? EInputGlyphDevice::Xbox : EInputGlyphDevice::KeyboardMouse;
	if (LoadedGlyphSet && PC)
	{
		if (const UInputGlyphSubsystem* DeviceSubsystem = ULocalPlayer::GetSubsystem<UInputGlyphSubsystem>(PC->GetLocalPlayer()))
		{
			Device = DeviceSubsystem->GetDevice();
			bGamepad = Device != EInputGlyphDevice::KeyboardMouse;
		}
	}

	FKey Key = ResolveActionKey(this, PC, PromptAction, FallbackMappingContext, KeyboardFallbackText, bGamepad);
	// 현재 장치에 연결된 키가 없으면 다른 장치에서 실제로 입력할 수 있는 키를 안내한다.
	if (LoadedGlyphSet && !Key.IsValid())
	{
		Key = ResolveActionKey(this, PC, PromptAction, FallbackMappingContext, KeyboardFallbackText, !bGamepad);
		if (Key.IsValid())
		{
			bGamepad = !bGamepad;
			Device = bGamepad ? EInputGlyphDevice::Xbox : EInputGlyphDevice::KeyboardMouse;
		}
	}

	const FText Label = Key.IsValid() ? FormatKey(Key) : (bGamepad ? FText::GetEmpty() : KeyboardFallbackText);
	UTexture2D* Glyph = LoadedGlyphSet && Key.IsValid() ? LoadedGlyphSet->FindTexture(Key, Device) : nullptr;
	const FName IconName = GetIconNameForLabel(Label);
	const FRichImageRow* IconRow = bGamepad && GamepadIconTable && !IconName.IsNone()
		? GamepadIconTable->FindRow<FRichImageRow>(IconName, TEXT("InputPrompt"), false) : nullptr;
	const bool bShowIcon = GamepadIcon && (Glyph || IconRow);
	const bool bShowQTEGlyph = GamepadIcon && Glyph;
	if (GamepadIcon)
	{
		GamepadIcon->SetVisibility(bShowIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Glyph && GamepadIcon->GetBrush().GetResourceObject() != Glyph)
		{
			GamepadIcon->SetBrushFromTexture(Glyph);
			GamepadIcon->SetDesiredSizeOverride(FVector2D(72.0f, 72.0f));
			GamepadIcon->SetColorAndOpacity(FLinearColor::White);
		}
		else if (!Glyph && IconRow && GamepadIcon->GetBrush().GetResourceObject() != IconRow->Brush.GetResourceObject())
			GamepadIcon->SetBrush(IconRow->Brush);
	}
	KeyText->SetVisibility(bShowIcon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (KeyboardBackground)
	{
		// 키 이미지(QTE 글리프 포함)를 보여 줄 때는 바탕을 깔지 않아 이미지의 투명 영역이 그대로 비치게 한다.
		KeyboardBackground->SetVisibility(bShowIcon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Box && OriginalWidth > 0.0f)
	{
		// Keep the UMG badge's original size for one letter, expanding only for longer captions.
		float Width = bShowIcon ? OriginalWidth
			: FMath::Max(OriginalWidth, OriginalWidth * (0.35f + 0.65f * Label.ToString().Len()));
		if (bShowQTEGlyph)
		{
			Width = 88.0f;
			if (!FMath::IsNearlyEqual(Box->GetHeightOverride(), Width)) Box->SetHeightOverride(Width);
		}
		if (!FMath::IsNearlyEqual(Box->GetWidthOverride(), Width)) Box->SetWidthOverride(Width);
	}
	if (Label.EqualTo(LastLabel) && KeyText->GetText().EqualTo(Label)) return;
	LastLabel = Label;
	KeyText->SetText(Label);
}
