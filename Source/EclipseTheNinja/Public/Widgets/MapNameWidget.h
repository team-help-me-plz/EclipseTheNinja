#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapNameWidget.generated.h"

class UTextBlock;
class UWidgetAnimation;

/**
 * 맵 입장 시 맵 이름을 띄우는 위젯.
 *  - 생성되면 UMapNameSubsystem::OnShowMapName을 구독하고, 구독 전에 보류된 요청이 있으면 바로 받는다
 *  - 평소에는 Collapsed로 두어 레이아웃과 히트 테스트에서 빠진다
 *  - 알림을 받으면 HitTestInvisible로 보이게 한 뒤 MapNameText에 이름을 넣고 MapNameAmin 애니메이션을 처음부터 재생한다
 *  - 애니메이션이 끝나면 다시 Collapsed로 되돌린다
 *  - 레이아웃은 이 클래스를 부모로 한 WBP_MapNameWidget에서 만든다 (같은 이름의 텍스트·애니메이션이 자동 연결된다)
 */
UCLASS(Abstract)
class ECLIPSETHENINJA_API UMapNameWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void	NativeConstruct() override;
	virtual void	NativeDestruct() override;
	virtual void	OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;

	UFUNCTION()
	void	HandleShowMapName(const FText& InMapName);

	/** 이름을 넣고 애니메이션을 시작한 직후 호출된다. WBP에서 사운드 등 추가 연출을 붙일 때 쓴다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Map Name")
	void	BP_OnMapNameShown(const FText& InMapName);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock>	MapNameText = nullptr;

	/** 맵 이름 표시 애니메이션 (WBP의 애니메이션 이름과 같아야 연결된다) */
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation>	MapNameAmin = nullptr;
};
