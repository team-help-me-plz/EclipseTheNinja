#pragma once

#include "Engine/GameViewportClient.h"
#include "EclipseGameViewportClient.generated.h"

UCLASS()
class UEclipseGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	virtual ULocalPlayer* SetupInitialLocalPlayer(FString& OutError) override;
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;

#if WITH_EDITOR
private:
	bool RoutePIEPauseKey(const FInputKeyEventArgs& EventArgs);
#endif
};
