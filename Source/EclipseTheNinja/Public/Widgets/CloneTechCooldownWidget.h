

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
	UPROPERTY(Transient)
	TObjectPtr<UInputGlyphWidget> RecallKeyGlyph;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 능력 키 안내(AbilityKeyText·키 그림·게임패드 아이콘)를 보이거나 숨긴다.
	// 분신을 소환해 교환할 수 있는 동안은 교환 그림(CloneTechSwapImage)이 대신 보이므로 숨긴다.
	void	SetAbilityKeyPromptVisible(bool bInVisible);
	// 게임패드 아이콘은 키 안내가 보이는 상태에서 게임패드를 쓰는 동안에만 보인다
	void	RefreshGamepadKeyIcon();
	bool	bAbilityKeyPromptVisible = true;

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
