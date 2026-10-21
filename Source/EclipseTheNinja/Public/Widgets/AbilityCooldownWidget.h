

#pragma once

#include "CoreMinimal.h"
#include "Ability/AbilityInfo.h"

#include "Abilities/GameplayAbility.h"
#include "Blueprint/UserWidget.h"
#include "AbilityCooldownWidget.generated.h"

class UImage;
class UTextBlock;
class UGACooldownBase;

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UAbilityCooldownWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;

	virtual void	InitWidget(APawn* InPawn);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage>		CooldownImage = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock>	CooldownText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CooldownMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldown|Interval")
	float	CooldownUpdateInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	EAbilityInputID	TargetAbilityInputID = EAbilityInputID::None;

	virtual void	StartCooldown();
	virtual void	EndCooldown();

	virtual void	UpdateCooldown();
	virtual void	UpdateWidget(const float InRemainig, const float InDuration);

	virtual void	UnbindAbility();

	UPROPERTY(Transient)
	TWeakObjectPtr<UGACooldownBase> ObservedAbility;

private:

	FTimerHandle	CooldownUpdateTimer;

	FNumberFormattingOptions	DisplayOptions;

};
