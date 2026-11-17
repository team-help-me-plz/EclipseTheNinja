#pragma once

#include "CoreMinimal.h"
#include "Widgets/EclipseMenuWidget.h"
#include "GuideSignPopupWidget.generated.h"

class UButton;
class UTextBlock;
class UTexture;
class UGuideSignCardWidget;
class UGuideSignData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGuideSignPopupClosed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGuideSignPageChanged, int32, PageIndex);

/**
 * 안내 표지판 팝업. 카드 한 장에 페이지를 하나씩 보여 주고 이전/다음 버튼으로 넘긴다.
 * 표지판 하나의 여러 페이지(또는 동시에 닿은 여러 표지판의 페이지)가 이 위젯 하나에 이어 붙는다.
 * 열려 있는 동안 게임을 일시정지하고 UI 입력만 받는다.
 * 끝까지 읽어야 하는 페이지가 하나라도 있으면 마지막 페이지에 닿기 전까지 닫기(카드의 X, ESC)를 막는다.
 * 레이아웃은 이 클래스를 부모로 한 WBP에서 만든다. PageCard(WBP_GuideSignCard)는 필수, 나머지는 선택이다.
 */
UCLASS(Abstract)
class ECLIPSETHENINJA_API UGuideSignPopupWidget : public UEclipseMenuWidget
{
	GENERATED_BODY()

public:
	/** 화면에 열려 있는 팝업을 찾는다. 없으면 nullptr */
	static UGuideSignPopupWidget*	FindOpenPopup(const UObject* WorldContextObject);

	/**
	 * 페이지를 하나 뒤에 붙이고 그 페이지 번호를 돌려준다.
	 * bInMustReadToEnd면 마지막 페이지에 닿기 전까지 닫을 수 없다.
	 */
	UFUNCTION(BlueprintCallable, Category = "GuideSign")
	int32	AddPage(const UGuideSignData* InData, UTexture* InVideoTexture, bool bInMustReadToEnd);

	/** 뷰포트에 띄우고 게임을 일시정지한 뒤 UI 입력 모드로 바꾼다. 첫 페이지를 보여 준다. */
	UFUNCTION(BlueprintCallable, Category = "GuideSign")
	void	OpenPopup(APlayerController* InController);

	/** 팝업을 닫고 일시정지와 게임 입력 모드를 되돌린다. 닫기가 막혀 있어도 닫는다. */
	UFUNCTION(BlueprintCallable, Category = "GuideSign")
	void	ClosePopup();

	/** 닫기가 허용될 때만 닫는다 (카드의 X, ESC). */
	UFUNCTION(BlueprintCallable, Category = "GuideSign")
	void	TryClosePopup();

	/** 이전/다음 페이지로 넘긴다. 끝에서는 멈춘다. */
	UFUNCTION(BlueprintCallable, Category = "GuideSign")
	void	ShowPrevPage();

	UFUNCTION(BlueprintCallable, Category = "GuideSign")
	void	ShowNextPage();

	bool	IsOpen() const { return (this->bIsOpen); }
	int32	GetCurrentPage() const { return (this->CurrentPage); }

	/** 지금 닫을 수 있는지 (끝까지 읽어야 하는 페이지가 없거나 마지막 페이지에 닿았는지) */
	bool	CanClose() const;

	UPROPERTY(BlueprintAssignable, Category = "GuideSign")
	FOnGuideSignPopupClosed	OnPopupClosed;

	/** 보이는 페이지가 바뀔 때 (열릴 때 첫 페이지 포함). 표지판이 그 페이지의 동영상만 재생한다 */
	UPROPERTY(BlueprintAssignable, Category = "GuideSign")
	FOnGuideSignPageChanged	OnPageChanged;

protected:
	virtual void	NativeOnInitialized() override;
	/** ClosePopup을 거치지 않고 제거돼도(다른 UI 정리, 레벨 전환) 일시정지와 입력 모드를 되돌린다 */
	virtual void	NativeDestruct() override;
	/** ESC·게임패드 B로 닫기, 좌우 방향키·게임패드 좌우로 페이지 넘김 (이동 키 A/D와 키 반복은 무시) */
	virtual FReply	NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	/** 빈 곳을 클릭해도 키보드 포커스를 잃지 않게 해서 키 입력이 계속 동작하도록 한다 */
	virtual FReply	NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** 열릴 때 호출된다. 일시정지 중이므로 WBP 애니메이션은 UI 시간으로 재생된다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GuideSign")
	void	BP_OnPopupOpened();

	/** 페이지가 바뀐 뒤 호출된다. 넘김 연출을 붙인다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GuideSign")
	void	BP_OnPageShown(int32 InPageIndex, int32 InPageCount);

	UFUNCTION()
	void	HandleCardCloseRequested(UGuideSignCardWidget* InCard);

	UFUNCTION()
	void	HandlePrevClicked();

	UFUNCTION()
	void	HandleNextClicked();

	/** 페이지를 보여 주는 카드 (WBP_GuideSignCard를 넣고 이 이름으로 둔다) */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UGuideSignCardWidget>	PageCard = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton>	PrevButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton>	NextButton = nullptr;

	/** "1 / 4" 같은 페이지 표시 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock>	PageText = nullptr;

	/** 다른 UI보다 위에 그리기 위한 ZOrder */
	UPROPERTY(EditDefaultsOnly, Category = "GuideSign")
	int32	ViewportZOrder = 100;

private:
	struct FPage
	{
		TWeakObjectPtr<const UGuideSignData>	Data;
		TWeakObjectPtr<UTexture>	VideoTexture;
	};

	/** Index번 페이지를 카드에 채우고 버튼·페이지 표시·닫기 가능 여부를 갱신한다 */
	void	ShowPage(int32 InIndex);

	/** 일시정지·입력 모드를 되돌리고 닫힘을 알린다. 이미 닫혔으면 아무것도 하지 않는다 */
	void	RestoreAndNotify();

	TArray<FPage>	Pages;
	int32	CurrentPage = INDEX_NONE;
	/** 끝까지 읽어야 하는 페이지가 하나라도 있는지 */
	bool	bMustReadToEnd = false;
	/** 마지막 페이지에 한 번이라도 닿았는지 */
	bool	bReachedLastPage = false;

	TWeakObjectPtr<APlayerController>	OwningController;
	bool	bIsOpen = false;
	/** 열기 전에 이미 다른 곳이 게임을 멈춰 두었는지 (그 일시정지는 닫을 때 풀지 않는다) */
	bool	bWasPausedBefore = false;
	/** 열기 전에 마우스 커서가 보였는지 (닫을 때 원래대로 되돌린다) */
	bool	bPrevShowMouseCursor = false;
};
