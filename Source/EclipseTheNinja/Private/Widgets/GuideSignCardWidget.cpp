#include "Widgets/GuideSignCardWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture.h"
#include "Level/GuideSignData.h"

void UGuideSignCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(this->CardCloseButton))
	{
		this->CardCloseButton->OnClicked.AddUniqueDynamic(this, &UGuideSignCardWidget::HandleCloseClicked);
	}
}

void UGuideSignCardWidget::SetCloseEnabled(bool bInEnabled)
{
	if (IsValid(this->CardCloseButton))
	{
		this->CardCloseButton->SetIsEnabled(bInEnabled);
	}

	// 꺼진 버튼에 외곽선이 남으면 눌러도 되는 것처럼 보이므로 함께 숨긴다
	if (IsValid(this->CardCloseOutline))
	{
		const ESlateVisibility OutlineVisibility = bInEnabled
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden;
		this->CardCloseOutline->SetVisibility(OutlineVisibility);
	}
}

void UGuideSignCardWidget::RequestClose()
{
	this->OnCloseRequested.Broadcast(this);
}

void UGuideSignCardWidget::HandleCloseClicked()
{
	this->RequestClose();
}

void UGuideSignCardWidget::SetupCard(const UGuideSignData* InData, UTexture* InVideoTexture)
{
	const FText Title = InData ? InData->Title : FText::GetEmpty();
	const FText Body = InData ? InData->Body : FText::GetEmpty();
	const bool bHasVideo = IsValid(InVideoTexture);

	// 비어 있는 칸은 접어서 남은 칸이 위로 붙게 한다
	const ESlateVisibility TitleVisibility = Title.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::HitTestInvisible;
	const ESlateVisibility BodyVisibility = Body.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::HitTestInvisible;
	const ESlateVisibility VideoVisibility = bHasVideo
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;

	if (IsValid(this->TitleText))
	{
		this->TitleText->SetText(Title);
		this->TitleText->SetVisibility(TitleVisibility);
	}

	if (IsValid(this->BodyText))
	{
		this->BodyText->SetText(Body);
		this->BodyText->SetVisibility(BodyVisibility);
	}

	// 브러시 크기는 WBP에서 정한 값을 유지하고 리소스만 미디어 텍스처로 바꾼다
	if (IsValid(this->VideoImage))
	{
		if (bHasVideo)
		{
			this->VideoImage->SetBrushResourceObject(InVideoTexture);
		}

		this->VideoImage->SetVisibility(VideoVisibility);
	}

	if (IsValid(this->VideoFrame))
	{
		this->VideoFrame->SetVisibility(VideoVisibility);
	}

	BP_OnCardSetup(bHasVideo);
}
