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
		Controller->SetInputMode(FInputModeGameOnly());
		Controller->bShowMouseCursor = this->bPrevShowMouseCursor;
		// 위젯이 떠 있는 동안 누르고 있던 키가 닫힌 뒤 게임 입력으로 이어지지 않게 비운다
		Controller->FlushPressedKeys();
	}
	UGameplayStatics::SetGamePaused(this, false);

	RemoveFromParent();
	this->OnNoticeClosed.Broadcast();
}

FReply UAbilityUnlockNoticeWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
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
