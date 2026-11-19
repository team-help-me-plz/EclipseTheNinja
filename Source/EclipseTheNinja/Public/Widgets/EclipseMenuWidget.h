#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EclipseMenuWidget.generated.h"

class USoundBase;

/**
 * 메뉴 위젯 공용 부모. 버튼 호버·선택 사운드를 WBP 기본값에서 등록한다.
 */
UCLASS(Abstract)
class ECLIPSETHENINJA_API UEclipseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USoundBase*	GetHoverSound() const { return this->HoverSound; }
	USoundBase*	GetSelectSound() const { return this->SelectSound; }

protected:
	// 마우스 호버와 키보드·게임패드 포커스 이동 때 재생한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase>	HoverSound = nullptr;

	// 버튼을 누를 때(클릭·Enter·게임패드 A) 재생한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase>	SelectSound = nullptr;
};
