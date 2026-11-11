#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/MenuInputFeedback.h"
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
	// 입력 장치 추적을 시작하고, 메뉴를 연 입력이 게임패드였는지 반영한다.
	virtual void NativeConstruct() override;

	// 입력 장치 추적을 멈추고 버튼 스타일을 되돌린다.
	virtual void NativeDestruct() override;

	// 게임패드 포커스 버튼을 붉은색으로 칠한다.
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

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
	UButton* AddMenuButton(UVerticalBox& Menu, const FText& Label);

	// 방향키패드 위아래로 메뉴 버튼 포커스를 옮긴다(양 끝에서 반대쪽으로 돈다).
	void MoveButtonFocus(int32 Direction);

	// 메뉴 버튼에 게임패드 포커스를 준다.
	void FocusMenuButton(int32 Index);

	// 위에서 아래 순서의 메뉴 버튼(재개, 설정, 메인 화면).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> MenuButtons;

	// 메뉴가 다시 포커스를 받을 때 고를 버튼 인덱스(설정에서 돌아오면 1 = 설정 버튼).
	int32 PreferredButtonIndex = 0;

	// 입력 장치 추적과 포커스 버튼 색 적용.
	FMenuInputFeedback InputFeedback;

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleMainMenuClicked();
};
