

#pragma once

#include "CoreMinimal.h"
#include "Widgets/AbilityCooldownWidget.h"
#include "CloneTechCooldownWidget.generated.h"

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UCloneTechCooldownWidget : public UAbilityCooldownWidget
{
	GENERATED_BODY()

public:
	virtual void	NativeConstruct() override;

	virtual void	InitWidget(APawn* InPawn) override;

protected:
	void	CloneSpawnedUpdate();
	void	CloneSwapCannotUpdate();

	virtual void	StartCooldown() override;
	virtual void	EndCooldown() override;
	virtual void	UnbindAbility() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage>		CloneTechSwapImage = nullptr;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage>		CloneTechSwapRecallUsable = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock>	AbilityKeyText = nullptr;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage>		CloneTechRecallUsable = nullptr;

};
