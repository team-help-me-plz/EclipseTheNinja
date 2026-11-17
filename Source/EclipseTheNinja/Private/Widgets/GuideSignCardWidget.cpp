#include "Widgets/GuideSignCardWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture.h"
#include "Level/GuideSignData.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystem/InputGlyphSubsystem.h"

namespace
{
	/** 게임패드 키를 패드 종류에 맞는 버튼 이름으로 바꾼다. 게임패드 키가 아니면 엔진 표시 이름을 쓴다 */
	FString	GetKeyLabel(const FKey& InKey, EInputGlyphDevice InDevice)
	{
		const bool	bPS = (InDevice == EInputGlyphDevice::PlayStation);

		if (InKey == EKeys::Gamepad_FaceButton_Bottom) return (bPS ? TEXT("×") : TEXT("A"));
		if (InKey == EKeys::Gamepad_FaceButton_Right) return (bPS ? TEXT("○") : TEXT("B"));
		if (InKey == EKeys::Gamepad_FaceButton_Left) return (bPS ? TEXT("□") : TEXT("X"));
		if (InKey == EKeys::Gamepad_FaceButton_Top) return (bPS ? TEXT("△") : TEXT("Y"));
		if (InKey == EKeys::Gamepad_LeftShoulder) return (bPS ? TEXT("L1") : TEXT("LB"));
		if (InKey == EKeys::Gamepad_RightShoulder) return (bPS ? TEXT("R1") : TEXT("RB"));
		if (InKey == EKeys::Gamepad_LeftTrigger || InKey == EKeys::Gamepad_LeftTriggerAxis) return (bPS ? TEXT("L2") : TEXT("LT"));
		if (InKey == EKeys::Gamepad_RightTrigger || InKey == EKeys::Gamepad_RightTriggerAxis) return (bPS ? TEXT("R2") : TEXT("RT"));
		if (InKey == EKeys::Gamepad_Special_Left) return (bPS ? TEXT("Create") : TEXT("View"));
		if (InKey == EKeys::Gamepad_Special_Right) return (bPS ? TEXT("Options") : TEXT("Menu"));
		if (InKey == EKeys::Gamepad_DPad_Up) return (TEXT("D패드 위"));
		if (InKey == EKeys::Gamepad_DPad_Down) return (TEXT("D패드 아래"));
		if (InKey == EKeys::Gamepad_DPad_Left) return (TEXT("D패드 왼쪽"));
		if (InKey == EKeys::Gamepad_DPad_Right) return (TEXT("D패드 오른쪽"));
		if (InKey == EKeys::Gamepad_Left2D || InKey == EKeys::Gamepad_LeftX || InKey == EKeys::Gamepad_LeftY) return (TEXT("왼쪽 스틱"));
		if (InKey == EKeys::Gamepad_Right2D || InKey == EKeys::Gamepad_RightX || InKey == EKeys::Gamepad_RightY) return (TEXT("오른쪽 스틱"));
		if (InKey == EKeys::Gamepad_LeftThumbstick) return (bPS ? TEXT("L3") : TEXT("LS"));
		if (InKey == EKeys::Gamepad_RightThumbstick) return (bPS ? TEXT("R3") : TEXT("RS"));
		return (InKey.GetDisplayName().ToString());
	}

	/** {키 이름}을 버튼 이름으로 바꾼다. 키 이름이 아닌 중괄호는 그대로 둔다 */
	FText	ResolveKeyTokens(const FText& InText, EInputGlyphDevice InDevice)
	{
		const FString	Source = InText.ToString();
		if (!Source.Contains(TEXT("{")))
		{
			return (InText);
		}

		FString	Result;
		Result.Reserve(Source.Len());
		int32	Index = 0;
		while (Index < Source.Len())
		{
			const int32	Open = Source.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index);
			const int32	Close = (Open == INDEX_NONE)
				? INDEX_NONE
				: Source.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 1);
			if (Open == INDEX_NONE || Close == INDEX_NONE)
			{
				Result += Source.Mid(Index);
				break ;
			}
			Result += Source.Mid(Index, Open - Index);
			const FKey	Key(*Source.Mid(Open + 1, Close - Open - 1));
			Result += Key.IsValid() ? GetKeyLabel(Key, InDevice) : Source.Mid(Open, Close - Open + 1);
			Index = Close + 1;
		}
		return (FText::FromString(Result));
	}
}

void UGuideSignCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(this->CardCloseButton))
	{
		this->CardCloseButton->OnClicked.AddUniqueDynamic(this, &UGuideSignCardWidget::HandleCloseClicked);
	}

	// 팝업이 떠 있는 동안 패드를 잡거나 키보드로 돌아오면 본문의 키 설명을 바로 바꾼다
	const ULocalPlayer*	LocalPlayer = GetOwningLocalPlayer();
	if (!LocalPlayer)
	{
		const APlayerController*	PlayerController = UGameplayStatics::GetPlayerController(this, 0);
		LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	}
	if (LocalPlayer)
	{
		this->DeviceSubsystem = ULocalPlayer::GetSubsystem<UInputGlyphSubsystem>(LocalPlayer);
		if (this->DeviceSubsystem.IsValid())
		{
			this->DeviceChangedHandle = this->DeviceSubsystem->OnDeviceChanged.AddUObject(this, &UGuideSignCardWidget::HandleDeviceChanged);
		}
	}
}

void UGuideSignCardWidget::NativeDestruct()
{
	if (this->DeviceSubsystem.IsValid())
	{
		this->DeviceSubsystem->OnDeviceChanged.Remove(this->DeviceChangedHandle);
	}
	this->DeviceChangedHandle.Reset();
	this->DeviceSubsystem.Reset();

	Super::NativeDestruct();
}

EInputGlyphDevice UGuideSignCardWidget::GetInputDevice() const
{
	return (this->DeviceSubsystem.IsValid()
		? this->DeviceSubsystem->GetDevice()
		: EInputGlyphDevice::KeyboardMouse);
}

void UGuideSignCardWidget::HandleDeviceChanged()
{
	if (this->CurrentData.IsValid())
	{
		RefreshBody();
	}
}

void UGuideSignCardWidget::RefreshBody()
{
	if (!IsValid(this->BodyText))
	{
		return ;
	}

	const UGuideSignData*	Data = this->CurrentData.Get();
	const EInputGlyphDevice	Device = GetInputDevice();
	FText	Body = Data ? Data->Body : FText::GetEmpty();
	if (Data && Device != EInputGlyphDevice::KeyboardMouse && !Data->BodyGamepad.IsEmpty())
	{
		Body = Data->BodyGamepad;
	}

	// 비어 있는 칸은 접어서 남은 칸이 위로 붙게 한다
	this->BodyText->SetText(ResolveKeyTokens(Body, Device));
	this->BodyText->SetVisibility(Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
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
	const bool bHasVideo = IsValid(InVideoTexture);

	this->CurrentData = InData;

	// 비어 있는 칸은 접어서 남은 칸이 위로 붙게 한다
	const ESlateVisibility TitleVisibility = Title.IsEmpty()
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

	RefreshBody();

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
