#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "UObject/Interface.h"
#include "ParryDeflectable.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UParryDeflectable : public UInterface
{
    GENERATED_BODY()
};

/**
 * 패링으로 튕겨낼 수 있는 투사체가 구현한다.
 * 패링 판정 중인 플레이어가 이 투사체를 EffectCauser로 가진 피해를 받으면 피해 대신 Deflect가 호출된다.
 * Deflect는 피해 적용 도중에 불리므로 여기서는 표시만 해 두고, 피해 적용이 끝난 뒤 방향과 진영을 바꾼다.
 */
class ECLIPSETHENINJA_API IParryDeflectable
{
    GENERATED_BODY()

public:
    // 튕겨내기를 받아들이면 true. false이면 피해가 그대로 들어간다.
    virtual bool Deflect(AActor* Deflector) = 0;

protected:
    // 원래 피해량을 유지한 채 튕겨낸 쪽이 시전자, 투사체가 원인인 피해 Spec을 만든다.
    static FGameplayEffectSpecHandle MakeDeflectedDamageSpec(
        AActor* Deflector, AActor* Projectile, const FGameplayEffectSpecHandle& OriginalSpec);
};
