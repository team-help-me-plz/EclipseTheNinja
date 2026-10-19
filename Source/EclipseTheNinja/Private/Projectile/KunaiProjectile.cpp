#include "Projectile/KunaiProjectile.h"
#include "Character/Enemy.h"
#include "Character/PlayerCharacter.h"
#include "Clone/CloneCharacter.h"
#include "GAS/StatAttributeSet.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AKunaiProjectile::AKunaiProjectile()
{
    // 벽에 파고드는 짧은 구간에서만 Actor Tick을 사용한다. 비행은 ProjectileMovement가 담당한다.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    SetRootComponent(Collision);
    Collision->InitSphereRadius(16.0f);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Collision->SetGenerateOverlapEvents(false);
    Collision->CanCharacterStepUpOn = ECB_No;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Collision);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    Movement->SetUpdatedComponent(Collision);
    Movement->bAutoActivate = false;
    Movement->ProjectileGravityScale = 0.0f;
    Movement->bRotationFollowsVelocity = true;
    Movement->bShouldBounce = false;
    Movement->bConstrainToPlane = true;
    Movement->SetPlaneConstraintNormal(FVector::YAxisVector);
}

void AKunaiProjectile::InitializeKunai(
    UAbilitySystemComponent* InSourceASC,
    const FGameplayEffectSpecHandle& InDamageSpec,
    const FVector& Direction,
    float Speed)
{
    SourceASC = InSourceASC;
    DamageSpec = InDamageSpec;
    LaunchVelocity = Direction.GetSafeNormal() * FMath::Max(1.0f, Speed);
    bEnemyThrow = GetInstigator() && GetInstigator()->IsA<AEnemy>();
    bPlayerThrow = GetInstigator() && GetInstigator()->IsA<APlayerCharacter>();
    IgnoreThrowerCollision();
    if (bPlayerThrow)
    {
        // 비활성 풀 분신도 미리 제외하므로 발사 후 풀에서 활성화되어도 통과한다.
        for (TActorIterator<ACloneCharacter> It(GetWorld()); It; ++It) IgnoreFriendlyClone(*It);
    }
}

void AKunaiProjectile::IgnoreActorCollision(AActor* Actor)
{
    if (!IsValid(Actor)) return;
    Collision->IgnoreActorWhenMoving(Actor, true);
    TInlineComponentArray<UPrimitiveComponent*> Components;
    Actor->GetComponents(Components);
    for (UPrimitiveComponent* Component : Components)
    {
        Component->IgnoreActorWhenMoving(this, true);
        IgnoringCharacterComponents.AddUnique(Component);
    }
}

void AKunaiProjectile::IgnoreThrowerCollision()
{
    // 발사체와 캐릭터 양쪽의 이동 검사에서 서로를 제외한다.
    // Deferred Spawn 초기화와 BP Construction Script 이후에 모두 적용한다.
    IgnoreActorCollision(GetOwner());
    IgnoreActorCollision(GetInstigator());
}

void AKunaiProjectile::IgnoreFriendlyClone(AActor* Actor)
{
    if (bPlayerThrow && IsValid(Actor) && Actor->IsA<ACloneCharacter>()) IgnoreActorCollision(Actor);
}

float AKunaiProjectile::GetCollisionRadius() const
{
    return Collision->GetScaledSphereRadius();
}

void AKunaiProjectile::BeginPlay()
{
    Super::BeginPlay();

    // 발사자와의 초기 겹침은 무시하며, 빠른 쿠나이도 ProjectileMovement의 sweep으로 충돌한다.
    IgnoreThrowerCollision();
    if (bPlayerThrow)
    {
        for (TActorIterator<ACloneCharacter> It(GetWorld()); It; ++It) IgnoreFriendlyClone(*It);
        ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(
            FOnActorSpawned::FDelegate::CreateUObject(this, &AKunaiProjectile::IgnoreFriendlyClone));
    }
    // 메시가 BP에서 충돌/물리 설정을 덮어썼어도 판정은 전용 구체 하나로 제한한다.
    Mesh->SetSimulatePhysics(false);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Collision->OnComponentHit.AddDynamic(this, &AKunaiProjectile::OnProjectileHit);

    SetLifeSpan(FMath::Max(0.1f, Lifetime));

    Movement->SetPlaneConstraintOrigin(GetActorLocation());
    Movement->Velocity = LaunchVelocity;
    Movement->MaxSpeed = LaunchVelocity.Size();
    Movement->Activate();
}

void AKunaiProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (EmbeddedEnemy.IsValid()) EmbeddedEnemy->OnDestroyed.RemoveDynamic(this, &AKunaiProjectile::OnEmbeddedEnemyDestroyed);
    EmbeddedEnemy.Reset();
    if (ActorSpawnedHandle.IsValid() && GetWorld()) GetWorld()->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
    ActorSpawnedHandle.Reset();
    for (const TWeakObjectPtr<UPrimitiveComponent>& Component : IgnoringCharacterComponents)
    {
        if (Component.IsValid()) Component->IgnoreActorWhenMoving(this, false);
    }
    IgnoringCharacterComponents.Reset();
    Super::EndPlay(EndPlayReason);
}

void AKunaiProjectile::OnEmbeddedEnemyDestroyed(AActor* DestroyedActor)
{
    Destroy();
}

void AKunaiProjectile::EmbedInEnemy(AEnemy* Enemy, const FHitResult& Hit, const FVector& IncomingDirection)
{
    USkeletalMeshComponent* EnemyMesh = Enemy->GetMesh();
    if (!EnemyMesh || !EnemyMesh->GetSkinnedAsset())
    {
        Destroy();
        return;
    }

    FVector SurfaceLocation = Hit.ImpactPoint;
    FName BoneName = Hit.GetComponent() == EnemyMesh ? Hit.BoneName : NAME_None;
    if (BoneName.IsNone())
    {
        // 캡슐이 먼저 맞은 경우 대상 메시만 추가 검사해 피격 부위를 찾는다.
        FHitResult MeshHit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(KunaiEnemyBone), false);
        const float TraceDistance = EnemyMesh->Bounds.BoxExtent.GetMax() * 2.0f + GetCollisionRadius();
        if (EnemyMesh->LineTraceComponent(MeshHit, Hit.ImpactPoint - IncomingDirection * GetCollisionRadius(),
            Hit.ImpactPoint + IncomingDirection * TraceDistance, Params) && !MeshHit.BoneName.IsNone())
        {
            BoneName = MeshHit.BoneName;
            SurfaceLocation = MeshHit.ImpactPoint;
        }
        else
        {
            // 캡슐의 빈 공간에 맞았거나 메시 쿼리가 불가능하면 가장 가까운 Physics Asset 표면을 사용한다.
            FVector Normal;
            float Distance = 0.0f;
            if (!EnemyMesh->K2_GetClosestPointOnPhysicsAsset(Hit.ImpactPoint, SurfaceLocation, Normal, BoneName, Distance))
            {
                SurfaceLocation = Hit.ImpactPoint;
                BoneName = EnemyMesh->FindClosestBone(SurfaceLocation);
            }
        }
    }

    if (BoneName.IsNone())
    {
        Destroy();
        return;
    }
    SetActorTickEnabled(false);
    SetActorLocationAndRotation(SurfaceLocation + IncomingDirection * FMath::Max(0.0f, EnemyEmbedDistance),
        IncomingDirection.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
    // 월드 포즈를 유지한 채 본의 상대 좌표로 전환한다. 이후 애니메이션/이동은 본 부착이 처리한다.
    if (!AttachToComponent(EnemyMesh, FAttachmentTransformRules::KeepWorldTransform, BoneName))
    {
        Destroy();
        return;
    }
    EmbeddedEnemy = Enemy;
    Enemy->OnDestroyed.AddDynamic(this, &AKunaiProjectile::OnEmbeddedEnemyDestroyed);
    SetLifeSpan(FMath::Max(0.1f, EnemyEmbedLifetime));
}

void AKunaiProjectile::BeginWallEmbedding(const FVector& IncomingDirection)
{
    WallEmbedStart = GetActorLocation();
    WallEmbedEnd = WallEmbedStart + IncomingDirection * FMath::Max(0.0f, WallEmbedDistance);
    WallEmbedElapsed = 0.0f;

    if (WallEmbedDuration <= 0.0f || WallEmbedStart.Equals(WallEmbedEnd))
    {
        SetActorLocation(WallEmbedEnd, false);
        return;
    }
    SetActorTickEnabled(true);
}

void AKunaiProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    WallEmbedElapsed += DeltaSeconds;
    const float Alpha = FMath::Clamp(WallEmbedElapsed / FMath::Max(KINDA_SMALL_NUMBER, WallEmbedDuration), 0.0f, 1.0f);
    // 첫 충돌에서 충돌을 끈 뒤 이동하므로 벽 표면을 넘는 짧은 연출이 가능하다.
    SetActorLocation(FMath::Lerp(WallEmbedStart, WallEmbedEnd, Alpha), false);
    if (Alpha >= 1.0f) SetActorTickEnabled(false);
}

void AKunaiProjectile::OnProjectileHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    if (bConsumed
        || !IsValid(OtherActor)
        || OtherActor == GetOwner()
        || OtherActor == GetInstigator())
    {
        return;
    }

    bConsumed = true;
    FVector IncomingDirection = Movement->Velocity.GetSafeNormal();
    if (IncomingDirection.IsNearlyZero()) IncomingDirection = LaunchVelocity.GetSafeNormal();
    if (IncomingDirection.IsNearlyZero()) IncomingDirection = GetActorForwardVector();
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Movement->StopMovementImmediately();
    Movement->Deactivate();

    // 환경 표면은 기존 벽 박힘 연출을 적용한다.
    if (!OtherActor->IsA<APawn>())
    {
        SetLifeSpan(WallEmbedLifetime);
        BeginWallEmbedding(IncomingDirection);
        return;
    }

    const bool bHostile = bEnemyThrow
        ? (OtherActor->IsA<APlayerCharacter>() || OtherActor->IsA<ACloneCharacter>())
        : OtherActor->IsA<AEnemy>();
    UAbilitySystemComponent* TargetASC = bHostile
        ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor) : nullptr;

    // 피해 적용으로 사망 충돌이 해제되기 전에 피격 본을 찾는다.
    if (AEnemy* Enemy = Cast<AEnemy>(OtherActor)) EmbedInEnemy(Enemy, Hit, IncomingDirection);

    if (SourceASC.IsValid() && DamageSpec.IsValid()
        && TargetASC && TargetASC->GetSet<UStatAttributeSet>()
        && TargetASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) > 0.0f)
    {
        // 충돌 정보와 원래 시전자 Context를 함께 전달해 적의 피격 감지/경계 전환을 유지한다.
        DamageSpec.Data->GetContext().AddHitResult(Hit, true);
        SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data.Get(), TargetASC);
    }
}
