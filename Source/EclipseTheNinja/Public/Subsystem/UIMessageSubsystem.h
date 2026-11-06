

#pragma once

#include "CoreMinimal.h"

#include "Subsystems/LocalPlayerSubsystem.h"
#include "UIMessageSubsystem.generated.h"

class UUIMessageWidget;
class APlayerController;

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UUIMessageSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
	
public:
	void	RegisterUIMessageWidget(UUIMessageWidget* InWidget);
	static void	RegisterUIMessage(APlayerController* InPC, UUIMessageWidget* InWidget);

	// InHoldDuration이 0 이하이면 메시지 위젯의 기본 유지 시간을 쓴다
	UFUNCTION(BlueprintCallable, Category = "UIMessage")
	void	ShowMessageText(const FText& InText, float InHoldDuration = -1.f);

	static void	ShowMessage(APlayerController* InPC, const FText& InText, float InHoldDuration = -1.f);

protected:
	TObjectPtr<UUIMessageWidget>	TargetUIMessageWidget = nullptr;

};
