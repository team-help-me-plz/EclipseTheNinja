

#pragma once

#include "CoreMinimal.h"
#include "GAS/AbilityInfo.h"

#include "Abilities/GameplayAbility.h"
#include "Blueprint/UserWidget.h"
#include "AbilityCooldownWidget.generated.h"

class UImage;
class UTextBlock;
class UGACooldownBase;
class ABaseCharacter;

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UAbilityCooldownWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	virtual void	InitWidget(APawn* InPawn);

protected:
	/** 아직 갖지 않은 어빌리티면 위젯을 숨기고, 획득하는 순간 다시 보이게 한다. false면 항상 보인다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	bool	bHideUntilGranted = true;

	/** 어빌리티를 획득해 위젯이 나타날 때 호출된다 (시작부터 가진 경우는 호출되지 않음). 등장 애니메이션을 붙인다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability")
	void	BP_OnAbilityGranted();

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
	void	HandleAbilityGranted(EAbilityInputID InInputID);
	void	StopWaitingForGrant();
	void	SetAbilityVisible(bool bInVisible);

	// 어빌리티 획득을 기다리는 캐릭터
	TWeakObjectPtr<ABaseCharacter>	GrantWatchedCharacter;
	FDelegateHandle	AbilityGrantedHandle;
	// WBP에 지정된 원래 Visibility (다시 보이게 할 때 사용)
	ESlateVisibility	ShownVisibility = ESlateVisibility::SelfHitTestInvisible;

	FTimerHandle	CooldownUpdateTimer;

	FNumberFormattingOptions	DisplayOptions;

};
