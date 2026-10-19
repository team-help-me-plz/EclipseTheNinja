

#pragma once

#include "CoreMinimal.h"
#include "Widgets/UIMessageEntryWidget.h"

#include "Blueprint/UserWidget.h"
#include "UIMessageWidget.generated.h"

class UVerticalBox;

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UUIMessageWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void	MakeMessage(const FText& InText);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UIMessageEntry")
	float	MessageHoldDuration = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UIMessageEntry")
	TSubclassOf<UUIMessageEntryWidget>	MessageEntryClass = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UVerticalBox>			UIMessageVerticalBox = nullptr;

};
