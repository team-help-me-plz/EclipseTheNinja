#include "Widgets/InputPromptWidget.h"
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
	if (!Action) return bGamepad ? FText::GetEmpty() : KeyboardFallback;
	TArray<FKey> Keys;
	APlayerController* PC = Controller;
	if (!PC && WorldContext && WorldContext->GetWorld()) PC = UGameplayStatics::GetPlayerController(WorldContext, 0);
	if (PC && PC->GetLocalPlayer())
	{
		if (const auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			Keys = Subsystem->QueryKeysMappedToAction(Action);
	}
	const bool bHasDeviceKey = Keys.ContainsByPredicate([bGamepad](const FKey& Key) { return Key.IsGamepadKey() == bGamepad; });
	if (!bHasDeviceKey && MappingContext)
	{
		for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
			if (Mapping.Action == Action) Keys.AddUnique(Mapping.Key);
	}
	// Keep an existing E/R/mouse caption when it is still one of the real bindings.
	if (!bGamepad && !KeyboardFallback.IsEmpty())
		for (const FKey& Key : Keys)
			if (!Key.IsGamepadKey() && FormatKey(Key).EqualTo(KeyboardFallback)) return FormatKey(Key);
	for (const FKey& Key : Keys)
		if (Key.IsGamepadKey() == bGamepad) return FormatKey(Key);
	return bGamepad ? FText::GetEmpty() : KeyboardFallback;
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
	const APlayerCharacterController* PC = Cast<APlayerCharacterController>(GetOwningPlayer());
	if (!PC && GetWorld()) PC = Cast<APlayerCharacterController>(UGameplayStatics::GetPlayerController(this, 0));
	const bool bGamepad = PC && PC->IsUsingGamepad();
	const FText Label = GetPromptLabel(bGamepad);
	const FName IconName = GetIconNameForLabel(Label);
	const FRichImageRow* IconRow = bGamepad && GamepadIconTable && !IconName.IsNone()
		? GamepadIconTable->FindRow<FRichImageRow>(IconName, TEXT("InputPrompt"), false) : nullptr;
	const bool bShowIcon = GamepadIcon && IconRow;
	if (GamepadIcon)
	{
		GamepadIcon->SetVisibility(bShowIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShowIcon && GamepadIcon->GetBrush().GetResourceObject() != IconRow->Brush.GetResourceObject())
			GamepadIcon->SetBrush(IconRow->Brush);
	}
	KeyText->SetVisibility(bShowIcon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (KeyboardBackground)
		KeyboardBackground->SetVisibility(bShowIcon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (Box && OriginalWidth > 0.0f)
	{
		// Keep the UMG badge's original size for one letter, expanding only for longer captions.
		const float Width = bShowIcon ? OriginalWidth
			: FMath::Max(OriginalWidth, OriginalWidth * (0.35f + 0.65f * Label.ToString().Len()));
		if (!FMath::IsNearlyEqual(Box->GetWidthOverride(), Width)) Box->SetWidthOverride(Width);
	}
	if (Label.EqualTo(LastLabel) && KeyText->GetText().EqualTo(Label)) return;
	LastLabel = Label;
	KeyText->SetText(Label);
}
