

#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "ActivateHighlightComponent.generated.h"

class UMaterialInstance;

// Activatable 액터의 테두리를 켜고 끄는 컴포넌트
// 평소에는 테두리가 없고, 이 액터를 대상으로 하는 상호작용 액터가 선택되는 동안에만 붉은 테두리를 표시한다
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ECLIPSETHENINJA_API UActivateHighlightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UActivateHighlightComponent();

	// 여러 상호작용 액터가 같은 대상을 가리킬 수 있으므로 호출 횟수를 세어, 모두 꺼졌을 때만 테두리를 끈다
	void	ShowHighlight();
	void	HideHighlight();

	bool	IsHighlighted() const { return (this->ShowCount > 0); }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Highlight")
	TObjectPtr<UMaterialInstance>	ActivateHighlight = nullptr;

private:
	int32	ShowCount = 0;
};
