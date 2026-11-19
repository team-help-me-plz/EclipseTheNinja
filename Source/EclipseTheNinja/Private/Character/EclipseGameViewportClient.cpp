#include "Character/EclipseGameViewportClient.h"

#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"

#if WITH_EDITOR
#include "Character/PlayerCharacterController.h"
#include "Engine/Console.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "InputKeyEventArgs.h"
#endif

ULocalPlayer* UEclipseGameViewportClient::SetupInitialLocalPlayer(FString& OutError)
{
	ULocalPlayer* Player = Super::SetupInitialLocalPlayer(OutError);
	if (!Player) return nullptr;

	const UWorld* PlayWorld = GetWorld();
	if (!PlayWorld) return Player;
	if (PlayWorld->WorldType != EWorldType::Game) return Player;

	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return Player;

	// 최초 실행의 창 크기는 엔진에 맡기고, 저장값이 있으면 시작 옵션보다 우선한다.
	const FString Section = Settings->GetClass()->GetPathName();
	int32 SavedWidth = 0;
	int32 SavedHeight = 0;
	if (!GConfig->GetInt(*Section, TEXT("ResolutionSizeX"), SavedWidth, GGameUserSettingsIni)) return Player;
	if (!GConfig->GetInt(*Section, TEXT("ResolutionSizeY"), SavedHeight, GGameUserSettingsIni)) return Player;
	if (SavedWidth <= 0 || SavedHeight <= 0) return Player;

	Settings->ApplyResolutionSettings(false);
	return Player;
}

bool UEclipseGameViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
#if WITH_EDITOR
	if (RoutePIEPauseKey(EventArgs)) return true;
#endif
	return Super::InputKey(EventArgs);
}

#if WITH_EDITOR
bool UEclipseGameViewportClient::RoutePIEPauseKey(const FInputKeyEventArgs& EventArgs)
{
	if (EventArgs.Key != EKeys::Escape) return false;
	if (ViewportConsole && ViewportConsole->ConsoleActive()) return false;

	UWorld* PlayWorld = GetWorld();
	if (!PlayWorld) return false;
	if (PlayWorld->WorldType != EWorldType::PIE) return false;
	if (PlayWorld->GetNetMode() == NM_Client) return false;

	const FModifierKeysState Modifiers = FSlateApplication::Get().GetModifierKeys();
	if (Modifiers.IsAltDown() || Modifiers.IsControlDown() || Modifiers.IsShiftDown() || Modifiers.IsCommandDown())
	{
		return false;
	}

	UGameInstance* PlayInstance = PlayWorld->GetGameInstance();
	if (!PlayInstance) return false;

	APlayerCharacterController* Controller = Cast<APlayerCharacterController>(PlayInstance->GetFirstLocalPlayerController(PlayWorld));
	if (!Controller) return false;

	// 콤보 목록이 닫히며 포커스가 게임 뷰포트로 돌아와도 모달의 뒤로 가기를 우선한다.
	if (Controller->HasOpenPauseMenu())
	{
		if (EventArgs.Event == IE_Pressed)
		{
			Controller->HandlePauseMenuBack();
		}
		return true;
	}
	if (IgnoreInput()) return false;

	// PIE의 Stop 처리 전에 게임 컨트롤러의 기존 ESC 바인딩으로 전달한다.
	Controller->InputKey(EventArgs);
	return true;
}
#endif
