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

	// 메뉴 루트가 포커스를 받으면 게임패드 A로 누를 수 있게 버튼으로 넘긴다.
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;

	// 왼쪽 스틱의 자동 이동을 막는다(방향키패드 이동은 NativeOnPreviewKeyDown이 맡는다).
	virtual FNavigationReply NativeOnNavigation(
		const FGeometry& InGeometry,
		const FNavigationEvent& InNavigationEvent,
		const FNavigationReply& InDefaultReply
	) override;

private:
	UButton* AddMenuButton(UVerticalBox& Menu, const FText& Label, bool bPrimary = false);

	// 방향키패드 위아래로 메뉴 버튼 포커스를 옮긴다(양 끝에서 반대쪽으로 돈다).
	void MoveButtonFocus(int32 Direction);

	// 메뉴 버튼에 게임패드 포커스를 준다.
	void FocusMenuButton(int32 Index);

	// 위에서 아래 순서의 메뉴 버튼(재개, 설정, 메인 화면).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> MenuButtons;

	// 메뉴가 다시 포커스를 받을 때 고를 버튼 인덱스(설정에서 돌아오면 1 = 설정 버튼).
	int32 PreferredButtonIndex = 0;

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleMainMenuClicked();
};
