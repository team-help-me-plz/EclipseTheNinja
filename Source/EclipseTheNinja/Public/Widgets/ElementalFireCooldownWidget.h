#pragma once

#include "CoreMinimal.h"
#include "Widgets/AbilityCooldownWidget.h"
#include "ElementalFireCooldownWidget.generated.h"

/**
 * 불속성 어빌리티 위젯.
 * 사용 > 불이 붙어 있는 남은 시간 표시 > 불이 꺼지면 기존 쿨타임 표시 순서로 동작한다.
 */
UCLASS()
class ECLIPSETHENINJA_API UElementalFireCooldownWidget : public UAbilityCooldownWidget
{
	GENERATED_BODY()

public:
	virtual void	NativeConstruct() override;

	virtual void	InitWidget(APawn* InPawn) override;

protected:
	void	StartFireDuration();
	void	EndFireDuration();
	void	UpdateFireDuration();

	virtual void	EndCooldown() override;

	virtual void	UnbindAbility() override;

	// 불이 붙어 있는 동안 표시할 이미지. 머티리얼에 Progress 파라미터가 있으면 남은 비율을 넘긴다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage>		FireActiveImage = nullptr;

	// 불이 붙어 있는 동안 남은 시간을 표시할 텍스트
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage>		FireImage = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic>	FireActiveMaterial;

private:
	FTimerHandle	FireDurationUpdateTimer;

	FNumberFormattingOptions	FireDisplayOptions;
};
