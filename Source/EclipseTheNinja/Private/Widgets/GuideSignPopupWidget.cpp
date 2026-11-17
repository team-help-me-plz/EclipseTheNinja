#include "Widgets/GuideSignPopupWidget.h"

#include "Widgets/GuideSignCardWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Texture.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Level/GuideSignData.h"
#include "EclipseUISound.h"

UGuideSignPopupWidget* UGuideSignPopupWidget::FindOpenPopup(const UObject* WorldContextObject)
{
	TArray<UUserWidget*> Found;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		const_cast<UObject*>(WorldContextObject),
		Found,
		UGuideSignPopupWidget::StaticClass(),
		true
	);

	for (UUserWidget* Widget : Found)
	{
		UGuideSignPopupWidget* Popup = Cast<UGuideSignPopupWidget>(Widget);
		if (IsValid(Popup) && Popup->IsOpen())
		{
			return (Popup);
		}
	}

	return (nullptr);
}

void UGuideSignPopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 카드의 X와 이전/다음 버튼에 호버·선택 사운드를 넣는다 (카드 안의 버튼은 카드 위젯 트리에 있어 따로 넣는다)
	EclipseUISound::ApplyButtonSoundsToTree(this);
	if (IsValid(this->PageCard))
	{
		if (UButton* CardCloseButton = this->PageCard->GetCloseButton())
		{
			FButtonStyle Style = CardCloseButton->GetStyle();
			EclipseUISound::ApplyButtonSounds(Style, this);
			CardCloseButton->SetStyle(Style);
		}

		this->PageCard->OnCloseRequested.AddUniqueDynamic(this, &UGuideSignPopupWidget::HandleCardCloseRequested);
	}

	SetIsFocusable(true);

	if (IsValid(this->PrevButton))
	{
		this->PrevButton->OnClicked.AddUniqueDynamic(this, &UGuideSignPopupWidget::HandlePrevClicked);
	}

	if (IsValid(this->NextButton))
	{
		this->NextButton->OnClicked.AddUniqueDynamic(this, &UGuideSignPopupWidget::HandleNextClicked);
	}
}

int32 UGuideSignPopupWidget::AddPage(const UGuideSignData* InData, UTexture* InVideoTexture, bool bInMustReadToEnd)
{
	if (!IsValid(InData))
	{
		return (INDEX_NONE);
	}

	FPage Page;
	Page.Data = InData;
	Page.VideoTexture = InVideoTexture;
	const int32 PageIndex = this->Pages.Add(Page);

	// 열린 뒤에 페이지가 붙으면 이미 마지막 페이지를 봤더라도 새 페이지를 끝까지 봐야 한다
	if (bInMustReadToEnd)
	{
		this->bMustReadToEnd = true;
		this->bReachedLastPage = false;
	}

	// 이미 열려 있으면 페이지 표시와 버튼만 갱신한다
	if (this->bIsOpen)
	{
		this->ShowPage(this->CurrentPage);
	}

	return (PageIndex);
}

void UGuideSignPopupWidget::OpenPopup(APlayerController* InController)
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

	this->bWasPausedBefore = UGameplayStatics::IsGamePaused(this);
	UGameplayStatics::SetGamePaused(this, true);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InController->SetInputMode(InputMode);
	InController->bShowMouseCursor = true;
	SetKeyboardFocus();

	this->ShowPage(0);
	BP_OnPopupOpened();
}

bool UGuideSignPopupWidget::CanClose() const
{
	return (!this->bMustReadToEnd || this->bReachedLastPage);
}

void UGuideSignPopupWidget::ShowPage(int32 InIndex)
{
	if (this->Pages.IsEmpty())
	{
		return;
	}

	const int32 PrevPage = this->CurrentPage;
	this->CurrentPage = FMath::Clamp(InIndex, 0, this->Pages.Num() - 1);

	const int32 LastPage = this->Pages.Num() - 1;
	if (this->CurrentPage == LastPage)
	{
		this->bReachedLastPage = true;
	}

	if (IsValid(this->PageCard))
	{
		const FPage& Page = this->Pages[this->CurrentPage];
		this->PageCard->SetupCard(Page.Data.Get(), Page.VideoTexture.Get());
		this->PageCard->SetCloseEnabled(this->CanClose());
	}

	// 첫 페이지에서는 이전, 마지막 페이지에서는 다음을 막는다
	if (IsValid(this->PrevButton))
	{
		this->PrevButton->SetIsEnabled(this->CurrentPage > 0);
	}

	if (IsValid(this->NextButton))
	{
		this->NextButton->SetIsEnabled(this->CurrentPage < LastPage);
	}

	if (IsValid(this->PageText))
	{
		this->PageText->SetText(FText::FromString(FString::Printf(
			TEXT("%d / %d"),
			this->CurrentPage + 1,
			this->Pages.Num()
		)));
	}

	// 눌린 버튼이 막히며 포커스를 잃을 수 있으므로 키 입력이 계속 동작하도록 팝업에 포커스를 돌려준다
	SetKeyboardFocus();

	BP_OnPageShown(this->CurrentPage, this->Pages.Num());

	if (PrevPage != this->CurrentPage)
	{
		this->OnPageChanged.Broadcast(this->CurrentPage);
	}
}

void UGuideSignPopupWidget::ShowPrevPage()
{
	if (this->CurrentPage > 0)
	{
		this->ShowPage(this->CurrentPage - 1);
	}
}

void UGuideSignPopupWidget::ShowNextPage()
{
	if (this->CurrentPage < this->Pages.Num() - 1)
	{
		this->ShowPage(this->CurrentPage + 1);
	}
}

void UGuideSignPopupWidget::TryClosePopup()
{
	if (!this->CanClose())
	{
		return;
	}

	this->ClosePopup();
}

void UGuideSignPopupWidget::ClosePopup()
{
	if (!this->bIsOpen)
	{
		return;
	}

	RemoveFromParent();
	this->RestoreAndNotify();
}

void UGuideSignPopupWidget::NativeDestruct()
{
	this->RestoreAndNotify();

	Super::NativeDestruct();
}

void UGuideSignPopupWidget::RestoreAndNotify()
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
		// 팝업이 떠 있는 동안 누르고 있던 키가 닫힌 뒤 게임 입력으로 이어지지 않게 비운다
		Controller->FlushPressedKeys();
	}

	// 다른 곳이 먼저 건 일시정지는 그대로 둔다
	if (!this->bWasPausedBefore)
	{
		UGameplayStatics::SetGamePaused(this, false);
	}

	this->OnPopupClosed.Broadcast();
}

FReply UGuideSignPopupWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		// 끝까지 읽어야 할 때는 닫히지 않지만, 키 입력이 게임으로 새지 않게 처리한 것으로 둔다.
		// 버튼을 거치지 않고 닫으므로 선택 사운드를 직접 재생한다
		if (this->CanClose())
		{
			EclipseUISound::PlaySelectSound(this);
			this->ClosePopup();
		}

		return (FReply::Handled());
	}

	// 이동 키(A/D)는 쓰지 않는다. 걷다가 닿으면 누르고 있던 키로 페이지가 넘어가 버리기 때문이다
	const bool bPrevKey = (Key == EKeys::Left
		|| Key == EKeys::Gamepad_DPad_Left
		|| Key == EKeys::Gamepad_LeftShoulder);
	const bool bNextKey = (Key == EKeys::Right
		|| Key == EKeys::Gamepad_DPad_Right
		|| Key == EKeys::Gamepad_RightShoulder);

	if (!bPrevKey && !bNextKey)
	{
		return (Super::NativeOnKeyDown(InGeometry, InKeyEvent));
	}

	// 키를 누르고 있을 때 들어오는 반복 입력은 무시해 한 번 누르면 한 장만 넘어가게 한다
	if (InKeyEvent.IsRepeat())
	{
		return (FReply::Handled());
	}

	if (bPrevKey)
	{
		this->ShowPrevPage();
	}
	else
	{
		this->ShowNextPage();
	}

	return (FReply::Handled());
}

FReply UGuideSignPopupWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	return (FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse));
}

void UGuideSignPopupWidget::HandleCardCloseRequested(UGuideSignCardWidget* InCard)
{
	this->TryClosePopup();
}

void UGuideSignPopupWidget::HandlePrevClicked()
{
	this->ShowPrevPage();
}

void UGuideSignPopupWidget::HandleNextClicked()
{
	this->ShowNextPage();
}
