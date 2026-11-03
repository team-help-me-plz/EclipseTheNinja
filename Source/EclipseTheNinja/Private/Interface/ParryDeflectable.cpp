#include "Interface/ParryDeflectable.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

FGameplayEffectSpecHandle IParryDeflectable::MakeDeflectedDamageSpec(
    AActor* Deflector, AActor* Projectile, const FGameplayEffectSpecHandle& OriginalSpec)
{
    UAbilitySystemComponent* DeflectorASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Deflector);
    if (!DeflectorASC || !OriginalSpec.IsValid() || !OriginalSpec.Data->Def) return FGameplayEffectSpecHandle();

    FGameplayEffectContextHandle Context = DeflectorASC->MakeEffectContext();
    Context.AddInstigator(Deflector, Projectile);

    FGameplayEffectSpecHandle Spec = DeflectorASC->MakeOutgoingSpec(
        OriginalSpec.Data->Def->GetClass(), OriginalSpec.Data->GetLevel(), Context);
    if (!Spec.IsValid()) return Spec;

    const FName DamageName(TEXT("Damage"));
    Spec.Data->SetSetByCallerMagnitude(DamageName, OriginalSpec.Data->GetSetByCallerMagnitude(DamageName, false, 0.0f));
    return Spec;
}
