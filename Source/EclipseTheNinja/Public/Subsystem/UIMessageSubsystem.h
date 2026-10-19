

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

	UFUNCTION(BlueprintCallable, Category = "UIMessage")
	void	ShowMessageText(const FText& InText);

	static void	ShowMessage(APlayerController* InPC, const FText& InText);

protected:
	TObjectPtr<UUIMessageWidget>	TargetUIMessageWidget = nullptr;

};
