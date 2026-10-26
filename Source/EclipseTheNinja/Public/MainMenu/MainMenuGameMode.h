
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainMenuGameMode.generated.h"

class UMainMenuWidget;

/**
 * 메인 화면 레벨용 게임모드
 * 월드 세팅의 GameMode Override에 지정하면 메인 화면 위젯을 띄운다.
 */
UCLASS()
class ECLIPSETHENINJA_API AMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMainMenuGameMode();

	TSubclassOf<UMainMenuWidget>	GetMainMenuWidgetClass() const { return this->MainMenuWidgetClass; }

protected:
	// 비워두면 UMainMenuWidget 기본 레이아웃 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu")
	TSubclassOf<UMainMenuWidget>	MainMenuWidgetClass = nullptr;
};
