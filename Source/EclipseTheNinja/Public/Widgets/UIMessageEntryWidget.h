

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UIMessageEntryWidget.generated.h"

class UTextBlock;

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UUIMessageEntryWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	//void	
	void	SetEntryText(const FText& InText, const float InMessageHoldTime);
	void	PlayDisapear();

protected:
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock>			EntryText = nullptr;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation>	TextDisapearAnim = nullptr;

private:
	FTimerHandle	MessageHoldTimerHandle;

};
