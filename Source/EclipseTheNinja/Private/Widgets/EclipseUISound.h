#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"

class UUserWidget;

// 메뉴 버튼 공용 사운드 (호버 / 선택)
namespace EclipseUISound
{
	// 버튼 스타일에 호버·선택 사운드를 넣는다. 마우스 호버와 클릭·Enter 입력에서 Slate가 재생한다
	void	ApplyButtonSounds(FButtonStyle& Style);

	// 위젯 트리 안의 모든 버튼에 호버·선택 사운드를 넣는다. WBP에서 만든 버튼도 포함된다
	void	ApplyButtonSoundsToTree(UUserWidget* Owner);

	// 키보드·게임패드로 포커스를 옮겼을 때 호버 사운드를 재생한다
	void	PlayHoverSound();
}
