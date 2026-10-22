#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameEndCountdownWidget.generated.h"

class UTextBlock;

/**
 * 게임 종료 카운트다운 위젯.
 *  - 위젯 블루프린트 없이 C++에서 레이아웃을 직접 만든다
 *  - 화면 중앙에 안내 메시지와 남은 초를 크게 표시한다
 */
UCLASS()
class ECLIPSETHENINJA_API UGameEndCountdownWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 안내 메시지를 설정
	void	SetMessage(const FText& InMessage);

	// 남은 초를 설정. 0 이하면 숫자를 비운다
	void	SetCount(const int32 InCount);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	// 배경 + 메시지 + 숫자 레이아웃을 WidgetTree에 만든다
	void	BuildLayout();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock>	MessageText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock>	CountText = nullptr;

};
