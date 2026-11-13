

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HUDWidget.generated.h"

class UPlayerStatWidget;
class UAbilityCooldownContainerWidget;
class UCinematicSkipWidget;
class UWidget;
/**
 *
 */
UCLASS()
class ECLIPSETHENINJA_API UHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	public:
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void InitWidget(APawn* InPawn);

	// 건너뛸 수 있는 컷신 동안 켠다. 건너뛰기 위젯을 켜고, 같은 패널의 다른 HUD 위젯(체력바·쿨타임 등)은 숨겨 컷신 화면에서 내린다
	// 끄면 숨긴 위젯을 되돌린다. 입력·표시는 건너뛰기 위젯이 알아서 한다
	UFUNCTION(BlueprintCallable, Category = "Cinematic Skip")
	void SetCinematicSkipAvailable(bool bAvailable);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPlayerStatWidget> PlayerStatWidget;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UAbilityCooldownContainerWidget> AbilityCooldownContainerWidget;

	// 컷신 건너뛰기 위젯(WBP_CinematicSkip)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCinematicSkipWidget> CinematicSkipWidget;

private:
	// 건너뛸 수 있는 컷신 동안 숨긴 HUD 위젯과 원래 Visibility. 끌 때 되돌린다
	TMap<TWeakObjectPtr<UWidget>, ESlateVisibility> CinematicHiddenWidgets;
};
