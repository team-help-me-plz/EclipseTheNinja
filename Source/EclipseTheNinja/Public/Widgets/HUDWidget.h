

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HUDWidget.generated.h"

class UPlayerStatWidget;
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

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPlayerStatWidget> PlayerStatWidget;
};
