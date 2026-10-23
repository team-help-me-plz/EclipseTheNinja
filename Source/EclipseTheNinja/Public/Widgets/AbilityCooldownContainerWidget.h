

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

	// 표창 쿨타임 위젯. 테스트용 컨테이너 WBP에는 없을 수 있어 Optional로 바인딩한다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UAbilityCooldownWidget>	KunaiCooldown = nullptr;
};
