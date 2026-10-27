
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuPlayerController.generated.h"

class UMainMenuWidget;

/**
 * 메인 화면 위젯 생성 및 UI 전용 입력 모드 설정
 */
UCLASS()
class ECLIPSETHENINJA_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void	BeginPlay() override;

	UPROPERTY(Transient)
	TObjectPtr<UMainMenuWidget>	MainMenuWidget = nullptr;
};
