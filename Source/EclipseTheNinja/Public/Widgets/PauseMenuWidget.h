#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UButton;
class UVerticalBox;

UCLASS()
class UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

protected:
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UButton* AddMenuButton(UVerticalBox& Menu, const FText& Label, bool bPrimary = false);

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleMainMenuClicked();
};
