

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"
#include "PlayerStatWidget.generated.h"

class UStatBarWidget;
class UAbilitySystemComponent;

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UPlayerStatWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UStatBarWidget> HealthBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UStatBarWidget> ManaBar;

	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<UAbilitySystemComponent> ASC;

public:
	// 체력바 업데이트 함수
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void UpdateHealth(float CurrentVal, float MaxVal);

	// 위젯 초기화 함수 (바인딩, 초기 값 세팅)
	void InitWidget(APawn* InPawn);

protected:

	virtual void NativeDestruct() override;

	// 델리게이트 바인딩용 함수들
	void UpdateHealth(const FOnAttributeChangeData& InData);
	void UpdateMaxHealth(const FOnAttributeChangeData& InData);


private:
	void UnbindASC();

};
