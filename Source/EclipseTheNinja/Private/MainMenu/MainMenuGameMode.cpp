

#include "MainMenu/MainMenuGameMode.h"
#include "MainMenu/MainMenuPlayerController.h"

AMainMenuGameMode::AMainMenuGameMode()
{
	this->PlayerControllerClass = AMainMenuPlayerController::StaticClass();
	// 메인 화면에서는 조종할 캐릭터가 필요 없으므로 관전자로 시작
	this->bStartPlayersAsSpectators = true;
}
