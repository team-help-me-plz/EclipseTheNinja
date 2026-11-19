#pragma once

#include "CoreMinimal.h"
#include "Widgets/EclipseMenuWidget.h"
#include "AbilityUnlockNoticeWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAbilityUnlockNoticeClosed);

/**
 * 어빌리티 해금 안내 위젯.
 * 열려 있는 동안 게임을 일시정지하고 UI 입력만 받는다. 닫기 버튼이나 ESC로 닫으면 일시정지와 입력 모드를 되돌린다.
 * 레이아웃은 이 클래스를 부모로 한 WBP에서 만든다. CloseButton은 필수, 텍스트와 아이콘은 같은 이름으로 넣으면 자동 연결된다.
 */
UCLASS(Abstract)
class ECLIPSETHENINJA_API UAbilityUnlockNoticeWidget : public UEclipseMenuWidget
{
	GENERATED_BODY()

public:
	/** 표시할 내용을 채운다. 비어 있는 값은 해당 위젯을 숨긴다. */
	UFUNCTION(BlueprintCallable, Category = "AbilityUnlockNotice")
	void	SetupNotice(const FText& InTitle, const FText& InDescription, UTexture2D* InIcon);

	/** 뷰포트에 띄우고 게임을 일시정지한 뒤 UI 입력 모드로 바꾼다. */
	UFUNCTION(BlueprintCallable, Category = "AbilityUnlockNotice")
	void	OpenNotice(APlayerController* InController);

	/** 위젯을 닫고 일시정지와 게임 입력 모드를 되돌린다. */
	UFUNCTION(BlueprintCallable, Category = "AbilityUnlockNotice")
	void	CloseNotice();

	UPROPERTY(BlueprintAssignable, Category = "AbilityUnlockNotice")
	FOnAbilityUnlockNoticeClosed	OnNoticeClosed;

protected:
	virtual void	NativeOnInitialized() override;
	virtual FReply	NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	// 빈 곳을 클릭해도 키보드 포커스를 잃지 않게 해서 ESC가 계속 동작하도록 한다
	virtual FReply	NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** WBP 연출(페이드 등)을 붙일 수 있도록 열릴 때 호출된다. 일시정지 중이므로 애니메이션은 UI 시간으로 재생된다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AbilityUnlockNotice")
	void	BP_OnNoticeOpened();

	UFUNCTION()
	void	HandleCloseClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton>		CloseButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock>	TitleText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock>	DescriptionText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage>		IconImage = nullptr;

	/** 다른 UI보다 위에 그리기 위한 ZOrder */
	UPROPERTY(EditDefaultsOnly, Category = "AbilityUnlockNotice")
	int32	ViewportZOrder = 100;

private:
	TWeakObjectPtr<APlayerController>	OwningController;
	bool	bIsOpen = false;
	// 열기 전에 마우스 커서가 보였는지 (닫을 때 원래대로 되돌린다)
	bool	bPrevShowMouseCursor = false;
};
