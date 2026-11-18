#include "Widgets/AbilityUnlockNoticeWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EclipseUISound.h"

void UAbilityUnlockNoticeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 확인 버튼에 호버·선택 사운드를 넣는다
	EclipseUISound::ApplyButtonSoundsToTree(this);

	SetIsFocusable(true);
	if (IsValid(this->CloseButton))
	{
		this->CloseButton->OnClicked.AddUniqueDynamic(this, &UAbilityUnlockNoticeWidget::HandleCloseClicked);
	}
}

void UAbilityUnlockNoticeWidget::SetupNotice(const FText& InTitle, const FText& InDescription, UTexture2D* InIcon)
{
	if (IsValid(this->TitleText))
	{
		this->TitleText->SetText(InTitle);
		this->TitleText->SetVisibility(InTitle.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (IsValid(this->DescriptionText))
	{
		this->DescriptionText->SetText(InDescription);
		this->DescriptionText->SetVisibility(InDescription.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (IsValid(this->IconImage))
	{
		if (IsValid(InIcon))
		{
			this->IconImage->SetBrushFromTexture(InIcon);
			this->IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			this->IconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UAbilityUnlockNoticeWidget::OpenNotice(APlayerController* InController)
{
	if (this->bIsOpen || !IsValid(InController))
	{
		return;
	}
	this->bIsOpen = true;
	this->OwningController = InController;
	this->bPrevShowMouseCursor = InController->bShowMouseCursor;

	// 전체 화면을 클릭 대상으로 만들어 빈 곳 클릭이 게임 뷰포트로 빠지지 않게 한다
	SetVisibility(ESlateVisibility::Visible);
	AddToViewport(this->ViewportZOrder);

	UGameplayStatics::SetGamePaused(this, true);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InController->SetInputMode(InputMode);
	InController->bShowMouseCursor = true;
	SetKeyboardFocus();

	// 키 상태는 닫을 때가 아니라 열 때 비운다(GuideSignPopupWidget::OpenPopup과 같은 이유).
	// 닫을 때 비우면 떠 있는 동안 뗀 키가 Enhanced Input에 한 번 더 눌린 것으로 평가돼 줍는 키의 Started가 다시 난다
	InController->FlushPressedKeys();

	BP_OnNoticeOpened();
}

void UAbilityUnlockNoticeWidget::CloseNotice()
{
	if (!this->bIsOpen)
	{
		return;
	}
	this->bIsOpen = false;

	if (APlayerController* Controller = this->OwningController.Get())
	{
		// 게임은 평소 GameAndUI + 커서 표시로 돈다(PlayerCharacterController::ResumeFromPauseMenu와 같게).
		// GameOnly로 돌리면 클릭마다 뷰포트가 마우스 캡처를 잡고 놓느라 클릭이 한 번씩 먹히고 우클릭이 무시된다
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(InputMode);
		Controller->SetShowMouseCursor(this->bPrevShowMouseCursor);
		// 키 상태는 OpenNotice에서 이미 비웠다. 여기서 다시 비우면 줍는 키의 Started가 다시 난다
	}
	UGameplayStatics::SetGamePaused(this, false);

	RemoveFromParent();
	this->OnNoticeClosed.Broadcast();
}

FReply UAbilityUnlockNoticeWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		// 줍는 키(패드 B)를 누른 채로 있으면 그 반복 입력이 뜨자마자 알림을 닫으므로, 반복은 처리만 하고 닫지 않는다
		if (InKeyEvent.IsRepeat())
		{
			return FReply::Handled();
		}

		// 버튼을 거치지 않고 닫으므로 선택 사운드를 직접 재생한다
		EclipseUISound::PlaySelectSound(this);
		CloseNotice();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UAbilityUnlockNoticeWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse);
}

void UAbilityUnlockNoticeWidget::HandleCloseClicked()
{
	CloseNotice();
}
