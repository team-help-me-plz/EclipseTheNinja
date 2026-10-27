#include "GAS/Ability/GABossPatternAttack.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/Enemy.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GAS/Effects/GEBasicAttackDamage.h"
#include "GAS/StatAttributeSet.h"

void UGABossPatternAttack::OnPatternHit()
{
	AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsActive() || !Enemy || !SourceASC || Enemy->IsDead())
	{
		return;
	}

	if (!FMath::IsFinite(Damage) || Damage <= 0.0f)
	{
		return;
	}

	// 보스 앞쪽으로 판정 구를 쓸어낸다.
	const float Radius = FMath::Max(1.0f, AttackRadius);
	const FVector Forward = Enemy->GetActorForwardVector();
	const FVector Start = Enemy->GetActorLocation() + Forward * Radius;
	const FVector End = Enemy->GetActorLocation() + Forward * FMath::Max(Radius, AttackRange);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossPatternAttack), false, Enemy);

	TArray<FHitResult> Hits;
	Enemy->GetWorld()->SweepMultiByObjectType(
		Hits,
		Start,
		End,
		FQuat::Identity,
		ObjectParams,
		FCollisionShape::MakeSphere(Radius),
		QueryParams
	);

	if (bDrawDebugHit)
	{
		DrawDebugCapsule(
			Enemy->GetWorld(),
			(Start + End) * 0.5f,
			FVector::Dist(Start, End) * 0.5f + Radius,
			Radius,
			FRotationMatrix::MakeFromZ(Forward).ToQuat(),
			FColor::Red,
			false,
			1.0f
		);
	}

	// 한 번의 판정에서 같은 대상은 한 번만 맞는다. 적끼리는 공격하지 않도록 전투 대상만 맞춘다.
	TSet<AActor*> HitActors;
	AActor* CombatTarget = Enemy->GetCombatTarget();
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!IsValid(Target) || Target != CombatTarget || HitActors.Contains(Target))
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC || !TargetASC->GetSet<UStatAttributeSet>())
		{
			continue;
		}

		if (TargetASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f)
		{
			continue;
		}

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddHitResult(Hit);

		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
			UGEBasicAttackDamage::StaticClass(),
			GetAbilityLevel(),
			Context
		);

		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Damage")), Damage);
			HitActors.Add(Target);
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		}
	}
}
