

#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "AbilityCooldownContainerWidget.generated.h"

class UAbilityCooldownWidget;
/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UAbilityCooldownContainerWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void	InitWidget(APawn* InPawn);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UAbilityCooldownWidget>	CloneTechCooldown = nullptr;
};
