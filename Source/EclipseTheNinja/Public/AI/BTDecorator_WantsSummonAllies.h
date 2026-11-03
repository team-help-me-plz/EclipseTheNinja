#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_WantsSummonAllies.generated.h"

/**
 * UAllySummonComponent를 가진 적이 지금 동료를 불러야 하는지 검사한다.
 * 전투에 들어가고 아직 부르지 않았거나, 소리치는 중이면 true다. 컴포넌트가 없으면 항상 false다.
 * 블랙보드 키가 아니라 컴포넌트 상태를 보므로 매 틱 값을 비교해, 바뀌면 분기를 다시 평가한다.
 * Observer Aborts를 Lower Priority로 두면 일반 전투 서브트리를 실행하다가도 전투에 들어가는 순간 소환 분기로 넘어온다.
 */
UCLASS(meta = (DisplayName = "Wants Summon Allies"))
class ECLIPSETHENINJA_API UBTDecorator_WantsSummonAllies : public UBTDecorator
{
    GENERATED_BODY()

public:
    UBTDecorator_WantsSummonAllies();

protected:
    virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
    virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual uint16 GetInstanceMemorySize() const override;

private:
    // 적마다 따로 쓰는 직전 검사 결과
    struct FWantsSummonAlliesMemory
    {
        bool bLastResult = false;
    };
};
