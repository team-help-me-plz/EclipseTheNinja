#include "Projectile/BossSlashWave.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/PlayerCharacter.h"
#include "Clone/CloneCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GAS/StatAttributeSet.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

ABossSlashWave::ABossSlashWave()
{
	PrimaryActorTick.bCanEverTick = false;

	// 이동 sweep용 작은 구. 벽·지형에만 막힌다.
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(8.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetGenerateOverlapEvents(false);
	SetRootComponent(Collision);

	// 검기 크기의 피격 판정. Pawn과 겹치기만 한다.
	HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	HitBox->SetupAttachment(Collision);
	HitBox->InitBoxExtent(FVector(30.0f, 30.0f, 90.0f));
	HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitBox->SetCollisionObjectType(ECC_WorldDynamic);
	HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HitBox->SetGenerateOverlapEvents(true);

	Effect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Effect"));
	Effect->SetupAttachment(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->InitialSpeed = 0.0f;
	Movement->Velocity = FVector::ZeroVector;
}

void ABossSlashWave::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(Lifetime);

	HitBox->OnComponentBeginOverlap.AddDynamic(this, &ABossSlashWave::OnHitBoxOverlap);
	Movement->OnProjectileStop.AddDynamic(this, &ABossSlashWave::OnProjectileStopped);
}

void ABossSlashWave::InitializeSlashWave(
	UAbilitySystemComponent* InSourceASC,
	const FGameplayEffectSpecHandle& InDamageSpec,
	const FVector& Direction,
	float Speed
)
{
	SourceASC = InSourceASC;
	DamageSpec = InDamageSpec;
	DamagedActors.Reset();

	// 발사자 캡슐과 겹친 채 시작해도 판정은 OnHitBoxOverlap이 걸러낸다.
	const FVector SafeDirection = Direction.GetSafeNormal(UE_SMALL_NUMBER, GetActorForwardVector());
	SetActorRotation(SafeDirection.Rotation());
	Movement->Velocity = SafeDirection * FMath::Max(1.0f, Speed);
	Movement->UpdateComponentVelocity();

	// 스폰 순간 이미 겹쳐 있던 대상은 BeginOverlap이 다시 오지 않으므로 직접 검사한다.
	TArray<AActor*> OverlappingActors;
	HitBox->GetOverlappingActors(OverlappingActors);
	for (AActor* OtherActor : OverlappingActors)
	{
		TryDamage(OtherActor, FHitResult());
	}
}

void ABossSlashWave::OnHitBoxOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	TryDamage(OtherActor, SweepResult);
}

void ABossSlashWave::OnProjectileStopped(const FHitResult& ImpactResult)
{
	Dissipate(ImpactResult.bBlockingHit ? FVector(ImpactResult.ImpactPoint) : GetActorLocation());
}

void ABossSlashWave::TryDamage(AActor* OtherActor, const FHitResult& Hit)
{
	if (bDissipated || !IsValid(OtherActor) || OtherActor == GetOwner() || OtherActor == GetInstigator())
	{
		return;
	}

	// 보스의 검기는 플레이어와 분신만 맞힌다.
	if (!OtherActor->IsA<APlayerCharacter>() && !OtherActor->IsA<ACloneCharacter>())
	{
		return;
	}

	if (DamagedActors.Contains(OtherActor))
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
	if (!SourceASC.IsValid() || !DamageSpec.IsValid() || !TargetASC || !TargetASC->GetSet<UStatAttributeSet>())
	{
		return;
	}

	if (TargetASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f)
	{
		return;
	}

	DamagedActors.Add(OtherActor);

	// 원래 시전자 Context에 충돌 정보를 더해 피격 방향 처리에 쓰게 한다.
	if (Hit.bBlockingHit || Hit.GetActor())
	{
		DamageSpec.Data->GetContext().AddHitResult(Hit, true);
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data.Get(), TargetASC);

	if (!bPierceTargets)
	{
		Dissipate(OtherActor->GetActorLocation());
	}
}

void ABossSlashWave::Dissipate(const FVector& Location)
{
	if (bDissipated)
	{
		return;
	}

	bDissipated = true;

	if (DissipateEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			DissipateEffect,
			Location,
			GetActorRotation()
		);
	}

	Destroy();
}
