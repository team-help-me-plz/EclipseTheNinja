#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/InputGlyphSet.h"
#include "GuideSignCardWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture;
class UGuideSignData;
class UGuideSignCardWidget;
class UInputGlyphSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGuideSignCardCloseRequested, UGuideSignCardWidget*, Card);

/**
 * 안내 표지판 팝업에서 페이지 하나를 보여 주는 카드 (제목, 내용, 동영상, 닫기 버튼).
 * 레이아웃은 이 클래스를 부모로 한 WBP에서 만든다. 아래 이름과 같은 위젯을 넣으면 자동 연결된다.
 * 카드 크기는 WBP의 SizeBox로 고정해 두어야 페이지마다 내용 길이가 달라도 크기가 흔들리지 않는다.
 */
UCLASS(Abstract)
class ECLIPSETHENINJA_API UGuideSignCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 표시할 내용을 채운다. 동영상 텍스처가 없으면 동영상 칸을 숨긴다. */
	UFUNCTION(BlueprintCallable, Category = "GuideSign")
	void	SetupCard(const UGuideSignData* InData, UTexture* InVideoTexture);

	/** 팝업을 닫아 달라고 알린다. 닫기 버튼을 누르면 호출된다. */
	UFUNCTION(BlueprintCallable, Category = "GuideSign")
	void	RequestClose();

	/** 닫기 버튼을 켜거나 끈다. 끝까지 읽어야 하는 동안 팝업이 끈다. */
	UFUNCTION(BlueprintCallable, Category = "GuideSign")
	void	SetCloseEnabled(bool bInEnabled);

	/** 팝업이 버튼 사운드를 넣을 수 있도록 닫기 버튼을 돌려준다. 없으면 nullptr */
	UButton*	GetCloseButton() const { return (this->CardCloseButton); }

	/** 닫기 요청. 팝업이 받아 닫을 수 있을 때만 닫는다 */
	UPROPERTY(BlueprintAssignable, Category = "GuideSign")
	FOnGuideSignCardCloseRequested	OnCloseRequested;

protected:
	virtual void	NativeOnInitialized() override;
	virtual void	NativeDestruct() override;

	/** 내용을 채운 뒤 호출된다. WBP에서 연출을 붙인다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GuideSign")
	void	BP_OnCardSetup(bool bHasVideo);

	UFUNCTION()
	void	HandleCloseClicked();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock>	TitleText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock>	BodyText = nullptr;

	/** 동영상을 그릴 이미지. 미디어 텍스처를 그대로 브러시로 쓴다 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage>	VideoImage = nullptr;

	/** 동영상이 없을 때 통째로 숨길 칸 (테두리 등). 없으면 VideoImage만 숨긴다 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget>	VideoFrame = nullptr;

	/** 팝업 닫기 버튼 (오른쪽 위 X). 끝까지 읽어야 하는 동안 꺼진다 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton>	CardCloseButton = nullptr;

	/** 닫기 버튼이 표지판 배경과 구분되도록 두르는 외곽선. 닫을 수 없는 동안에는 숨긴다 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget>	CardCloseOutline = nullptr;

private:
	/** 지금 입력 장치(키보드 / 게임패드)에 맞는 본문을 골라 키 이름을 채워 넣는다 */
	void	RefreshBody();

	/** 팝업이 떠 있는 동안 키보드 ↔ 게임패드가 바뀌면 본문을 다시 고른다 */
	void	HandleDeviceChanged();

	/** 입력 장치. 찾지 못하면 키보드로 본다 */
	EInputGlyphDevice	GetInputDevice() const;

	TWeakObjectPtr<const UGuideSignData>	CurrentData;

	TWeakObjectPtr<UInputGlyphSubsystem>	DeviceSubsystem;
	FDelegateHandle	DeviceChangedHandle;
};
