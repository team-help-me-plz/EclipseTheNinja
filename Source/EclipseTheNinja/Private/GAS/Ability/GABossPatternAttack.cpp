#include "GAS/Ability/GABossPatternAttack.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/BossEnemy.h"
#include "Character/Enemy.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/Animation/AnimNotify_BossAttackHit.h"
#include "GAS/Effects/GEBasicAttackDamage.h"
#include "GAS/StatAttributeSet.h"

void UGABossPatternAttack::OnPatternHit(const FBossAttackHitArea& HitArea)
{
	AEnemy* Enemy = Cast<AEnemy>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsActive() || !Enemy || !SourceASC || Enemy->IsDead())
	{
		return;
	}

	const float HitDamage = Damage * HitArea.DamageScale;
	if (!FMath::IsFinite(HitDamage) || HitDamage <= 0.0f)
	{
		return;
	}

	TArray<FHitResult> Hits;
	QueryHitArea(Enemy, HitArea, Hits);

	AActor* CombatTarget = Enemy->GetCombatTarget();
	const bool bTargetInArea = CombatTarget && Hits.ContainsByPredicate([CombatTarget](const FHitResult& Hit)
	{
		return Hit.GetActor() == CombatTarget;
	});

	DrawDebugHitArea(Enemy, HitArea, Hits, bTargetInArea);

	// 한 번의 판정에서 같은 대상은 한 번만 맞는다. 적끼리는 공격하지 않도록 전투 대상만 맞춘다.
	TSet<AActor*> HitActors;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!IsValid(Target) || Target != CombatTarget || HitActors.Contains(Target))
		{
			continue;
		}

		if (ApplyHitDamage(SourceASC, Target, Hit, HitDamage))
		{
			HitActors.Add(Target);
		}
	}
}

bool UGABossPatternAttack::ApplyHitDamage(
	UAbilitySystemComponent* SourceASC,
	AActor* Target,
	const FHitResult& Hit,
	float HitDamage
) const
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!SourceASC || !TargetASC || !TargetASC->GetSet<UStatAttributeSet>())
	{
		return false;
	}

	// 이미 쓰러진 대상은 다시 때리지 않는다.
	if (TargetASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f)
	{
		return false;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddHitResult(Hit);

	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
		UGEBasicAttackDamage::StaticClass(),
		GetAbilityLevel(),
		Context
	);

	if (!Spec.IsValid())
	{
		return false;
	}

	Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Damage")), HitDamage);
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	return true;
}

FVector UGABossPatternAttack::GetScaledHitExtent(const FTransform& ActorTransform, const FBossAttackHitArea& HitArea)
{
	return (HitArea.HalfExtent * ActorTransform.GetScale3D().GetAbs()).ComponentMax(FVector::OneVector);
}

void UGABossPatternAttack::QueryHitArea(
	const AEnemy* Enemy,
	const FBossAttackHitArea& HitArea,
	TArray<FHitResult>& OutHits
) const
{
	// 상자는 보스 액터 기준으로 놓고 보스가 바라보는 방향을 따라 돌린다. 위치와 크기 모두 액터 스케일을 따른다.
	const FTransform& ActorTransform = Enemy->GetActorTransform();
	const FVector Center = ActorTransform.TransformPosition(HitArea.Offset);
	const FQuat Rotation = ActorTransform.GetRotation();
	const FVector Extent = GetScaledHitExtent(ActorTransform, HitArea);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossPatternAttack), false, Enemy);

	TArray<FOverlapResult> Overlaps;
	Enemy->GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		Center,
		Rotation,
		ObjectParams,
		FCollisionShape::MakeBox(Extent),
		QueryParams
	);

	// 피해 Context에 넣을 수 있게 겹침 결과를 FHitResult로 바꾼다. 충돌 지점은 대상 위치로 둔다.
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!Actor)
		{
			continue;
		}

		const FVector HitLocation = Actor->GetActorLocation();
		OutHits.Emplace(
			Actor,
			Overlap.GetComponent(),
			HitLocation,
			(HitLocation - Center).GetSafeNormal()
		);
	}
}

void UGABossPatternAttack::DrawDebugHitArea(
	const AEnemy* Enemy,
	const FBossAttackHitArea& HitArea,
	const TArray<FHitResult>& Hits,
	bool bTargetInArea
) const
{
	const ABossEnemy* Boss = Cast<ABossEnemy>(Enemy);
	if (!Boss || !Boss->IsDrawDebugAttackEnabled())
	{
		return;
	}

	const UWorld* World = Enemy->GetWorld();
	const float Duration = Boss->GetDebugAttackDrawDuration();
	const FTransform& ActorTransform = Enemy->GetActorTransform();

	// 노티파이가 지정한 상자 영역
	DrawDebugBox(
		World,
		ActorTransform.TransformPosition(HitArea.Offset),
		GetScaledHitExtent(ActorTransform, HitArea),
		ActorTransform.GetRotation(),
		bTargetInArea ? FColor::Green : FColor::Red,
		false,
		Duration,
		0,
		2.0f
	);

	// 판정에 걸린 Pawn의 위치. 피해 대상이 아닌 Pawn도 함께 표시된다.
	for (const FHitResult& Hit : Hits)
	{
		DrawDebugPoint(
			World,
			Hit.ImpactPoint,
			12.0f,
			FColor::Yellow,
			false,
			Duration
		);
	}
}
