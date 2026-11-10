
#pragma once

#include "CoreMinimal.h"
#include "PuzzelObject/PressureSwitch.h"
#include "PlayerDetectTrap.generated.h"

/**
 * 압력 발판과 똑같이 작동하지만 테두리 강조가 없는 발판.
 * InteractHighlight 컴포넌트를 만들지 않고, 플레이어 InteractBox가 겹쳐도 발판과 대상의 테두리를 켜지 않는다
 */
UCLASS()
class ECLIPSETHENINJA_API APlayerDetectTrap : public APressureSwitch
{
	GENERATED_BODY()

public:
	APlayerDetectTrap(const FObjectInitializer& ObjectInitializer);

	virtual void	ShowInteractionIndicator_Implementation() override {}
	virtual void	HideInteractionIndicator_Implementation() override {}
};
