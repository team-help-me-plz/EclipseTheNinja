#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_IsOnCeiling.generated.h"

/**
 * 천장 매복 적이 아직 천장 단계(매달림·기습·공격 없는 낙하)인지 검사한다.
 * 착지해 일반 근접 적이 되면 false가 되어, 같은 Selector의 다음 분기(일반 전투 서브트리)로 넘어간다.
 * 천장 매복 적이 아니면 항상 false다.
 */
UCLASS(meta = (DisplayName = "Is On Ceiling"))
class ECLIPSETHENINJA_API UBTDecorator_IsOnCeiling : public UBTDecorator
{
    GENERATED_BODY()

public:
    UBTDecorator_IsOnCeiling();

protected:
    virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};
