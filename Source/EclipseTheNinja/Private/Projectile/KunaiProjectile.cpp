#include "Projectile/KunaiProjectile.h"
#include "Character/Enemy.h"
#include "Character/PlayerCharacter.h"
#include "Clone/CloneCharacter.h"
#include "GAS/StatAttributeSet.h"
#include "ObjectPool/ObjectPoolSubsystem.h"
#include "Subsystem/NoiseSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
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
    // 일반 스폰이면 BeginPlay 직후, 풀 스폰이면 재사용 직후이므로 이전 투척 상태를 먼저 비운다
    ClearThrowState();
    SetActorTickEnabled(false);
    bConsumed = false;

    // 충돌 시 끈 판정과 ProjectileMovement가 정지하며 해제한 UpdatedComponent를 복원
    Collision->SetCollisionEnabled(GetClass()->GetDefaultObject<AKunaiProjectile>()->Collision->GetCollisionEnabled());
    Movement->SetUpdatedComponent(Collision);

    SourceASC = InSourceASC;
    DamageSpec = InDamageSpec;
    LaunchVelocity = Direction.GetSafeNormal() * FMath::Max(1.0f, Speed);
    bEnemyThrow = GetInstigator() && GetInstigator()->IsA<AEnemy>();
    bPlayerThrow = GetInstigator() && GetInstigator()->IsA<APlayerCharacter>();

    // 손에서 생성될 때 발사자와 겹쳐 있어도 바로 충돌하지 않도록 발사자를 서로 무시
    IgnoreThrowerCollision();

    // 비활성 상태인 풀 안에 있는 분신 액터들도 미리 제외하여
    // 발사 후 분신이 소환되어도 통과되도록 설정
    if (bPlayerThrow)
    {
        for (TActorIterator<ACloneCharacter> It(GetWorld()); It; ++It)
        {
            IgnoreFriendlyActor(*It);
        }
    }
    else if (bEnemyThrow)
    {
        for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
        {
            IgnoreFriendlyActor(*It);
        }
    }
    // 진영과 무관하게 표창끼리는 충돌하지 않는다. 풀에 대기 중인 표창도 포함한다.
    for (TActorIterator<AKunaiProjectile> It(GetWorld()); It; ++It)
    {
        IgnoreFriendlyActor(*It);
    }
    ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(
        FOnActorSpawned::FDelegate::CreateUObject(this, &AKunaiProjectile::IgnoreFriendlyActor));

    SetLifeSpan(FMath::Max(0.1f, Lifetime));

    Movement->SetPlaneConstraintOrigin(GetActorLocation());
    Movement->Velocity = LaunchVelocity;
    Movement->MaxSpeed = LaunchVelocity.Size();
    Movement->Activate(true);
}

void AKunaiProjectile::IgnoreActorCollision(AActor* InActor)
{
    if (!IsValid(InActor) || InActor == this)
    {
        return;
    }

    Collision->IgnoreActorWhenMoving(InActor, true);

    TInlineComponentArray<UPrimitiveComponent*> Components;
    InActor->GetComponents(Components);
    for (UPrimitiveComponent* Component : Components)
    {
        Component->IgnoreActorWhenMoving(this, true);
        IgnoringCharacterComponents.AddUnique(Component);
    }
}

void AKunaiProjectile::IgnoreThrowerCollision()
{
    // 발사체와 캐릭터 양쪽의 이동 검사에서 서로를 제외한다.
    // 풀 재사용 시 Owner/Instigator가 바뀌므로 InitializeKunai에서 투척마다 다시 적용한다.
    IgnoreActorCollision(GetOwner());
    IgnoreActorCollision(GetInstigator());
}

void AKunaiProjectile::IgnoreFriendlyActor(AActor* InActor)
{
    if (IsValid(InActor) && (InActor->IsA<AKunaiProjectile>()
        || (bPlayerThrow && InActor->IsA<ACloneCharacter>())
        || (bEnemyThrow && InActor->IsA<AEnemy>())))
    {
        IgnoreActorCollision(InActor);
    }
}

float AKunaiProjectile::GetCollisionRadius() const
{
    return Collision->GetScaledSphereRadius();
}

bool AKunaiProjectile::SweepLaunchPath(
    const UWorld* World,
    const FVector& Start,
    const FVector& End,
    const FCollisionQueryParams& QueryParams,
    FHitResult& OutHit) const
{
    if (!World) return false;
    FCollisionQueryParams LaunchParams(QueryParams);
    // 비행 중뿐 아니라 손 앞에 다른 진영의 표창이 있는 경우에도 발사를 막지 않는다.
    for (TActorIterator<AKunaiProjectile> It(World); It; ++It)
    {
        LaunchParams.AddIgnoredActor(*It);
    }
    return World && World->SweepSingleByChannel(
        OutHit,
        Start,
        End,
        FQuat::Identity,
        Collision->GetCollisionObjectType(),
        FCollisionShape::MakeSphere(FMath::Max(1.0f, GetCollisionRadius())),
        LaunchParams,
        FCollisionResponseParams(Collision->GetCollisionResponseToChannels())
    );
}

void AKunaiProjectile::HandleLaunchHit(const FHitResult& Hit)
{
    OnProjectileHit(Collision, Hit.GetActor(), Hit.GetComponent(), FVector::ZeroVector, Hit);
}

void AKunaiProjectile::BeginPlay()
{
    Super::BeginPlay();

    // 비행은 InitializeKunai에서 시작한다. 풀 액터는 생성 시 한 번만 BeginPlay가 호출된다.
    // 메시가 BP에서 충돌/물리 설정을 덮어썼어도 판정은 전용 구체 하나로 제한한다.
    Mesh->SetSimulatePhysics(false);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Collision->OnComponentHit.AddDynamic(this, &AKunaiProjectile::OnProjectileHit);
}

void AKunaiProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearThrowState();
    Super::EndPlay(EndPlayReason);
}

void AKunaiProjectile::LifeSpanExpired()
{
    UObjectPoolSubsystem::Release(this);
}

void AKunaiProjectile::OnReturnToPool_Implementation()
{
    // 공통 비활성화(숨김/충돌/틱/타이머/대기 위치 이동) 전에 부착을 풀어 대상과 함께 이동하지 않게 한다.
    ClearThrowState();
    Movement->StopMovementImmediately();
    Movement->Deactivate();
}

void AKunaiProjectile::ClearThrowState()
{
    if (AActor* Attached = AttachedActor.Get())
    {
        Attached->OnDestroyed.RemoveDynamic(this, &AKunaiProjectile::OnAttachedActorDestroyed);
    }
    AttachedActor.Reset();

    UObjectPoolSubsystem* PoolSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UObjectPoolSubsystem>() : nullptr;
    if (PoolSubsystem && PoolReturnHandle.IsValid())
    {
        PoolSubsystem->OnActorReturnedToPool.Remove(PoolReturnHandle);
    }
    PoolReturnHandle.Reset();
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    if (ActorSpawnedHandle.IsValid() && GetWorld())
    {
        GetWorld()->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
    }
    ActorSpawnedHandle.Reset();

    for (const TWeakObjectPtr<UPrimitiveComponent>& Component : IgnoringCharacterComponents)
    {
        if (Component.IsValid())
        {
            Component->IgnoreActorWhenMoving(this, false);
        }
    }
    IgnoringCharacterComponents.Reset();
    Collision->ClearMoveIgnoreActors();

    SourceASC.Reset();
    DamageSpec.Clear();
}

void AKunaiProjectile::OnAttachedActorDestroyed(AActor* DestroyedActor)
{
    UObjectPoolSubsystem::Release(this);
}

void AKunaiProjectile::OnPooledActorReturned(AActor* ReturnedActor)
{
    if (ReturnedActor && ReturnedActor == AttachedActor.Get())
    {
        UObjectPoolSubsystem::Release(this);
    }
}

void AKunaiProjectile::TrackAttachedActor(AActor* InActor)
{
    if (!IsValid(InActor) || AttachedActor.Get() == InActor)
    {
        return;
    }

    AttachedActor = InActor;
    InActor->OnDestroyed.AddDynamic(this, &AKunaiProjectile::OnAttachedActorDestroyed);

    if (UObjectPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<UObjectPoolSubsystem>())
    {
        PoolReturnHandle = PoolSubsystem->OnActorReturnedToPool.AddUObject(this, &AKunaiProjectile::OnPooledActorReturned);
    }
}

FTransform AKunaiProjectile::GetAttachBasis() const
{
    const USceneComponent* Parent = GetRootComponent()->GetAttachParent();
    return Parent ? Parent->GetSocketTransform(GetRootComponent()->GetAttachSocketName()) : FTransform::Identity;
}

FKunaiAttachTarget AKunaiProjectile::ResolveAttachTarget(AActor* OtherActor, const FHitResult& Hit, const FVector& IncomingDirection) const
{
    // 캐릭터는 캡슐 대신 메시의 피격 본에 붙여 애니메이션을 따라가게 한다
    FKunaiAttachTarget Target;
    if (const ACharacter* Character = Cast<ACharacter>(OtherActor))
    {
        if (ResolveCharacterAttachTarget(Character->GetMesh(), Hit, IncomingDirection, Target))
        {
            return Target;
        }
    }

    // 벽/발판/문 등은 맞은 컴포넌트에 붙는다. 본이 있는 메시면 해당 본을 사용한다
    USceneComponent* Surface = Hit.GetComponent();
    Target.Parent = Surface;
    Target.SocketName = Surface && Surface->DoesSocketExist(Hit.BoneName) ? Hit.BoneName : NAME_None;
    Target.SurfacePoint = Hit.ImpactPoint;

    return Target;
}

bool AKunaiProjectile::ResolveCharacterAttachTarget(
    USkeletalMeshComponent* CharacterMesh,
    const FHitResult& Hit,
    const FVector& IncomingDirection,
    FKunaiAttachTarget& OutTarget) const
{
    if (!CharacterMesh || !CharacterMesh->GetSkinnedAsset())
    {
        return false;
    }

    FVector SurfacePoint = Hit.ImpactPoint;
    FName BoneName = Hit.GetComponent() == CharacterMesh ? Hit.BoneName : NAME_None;
    if (BoneName.IsNone())
    {
        // 캡슐이 먼저 맞은 경우 대상 메시만 추가 검사해 피격 부위를 찾는다
        FHitResult MeshHit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(KunaiCharacterBone), false);
        const float TraceDistance = CharacterMesh->Bounds.BoxExtent.GetMax() * 2.0f + GetCollisionRadius();
        const bool bHit = CharacterMesh->LineTraceComponent(
            MeshHit,
            Hit.ImpactPoint - IncomingDirection * GetCollisionRadius(),
            Hit.ImpactPoint + IncomingDirection * TraceDistance,
            Params
        );

        if (bHit && !MeshHit.BoneName.IsNone())
        {
            BoneName = MeshHit.BoneName;
            SurfacePoint = MeshHit.ImpactPoint;
        }
        else
        {
            // 캡슐의 빈 공간에 맞았거나 메시 쿼리가 불가능하면 가장 가까운 Physics Asset 표면을 사용한다
            FVector Normal;
            float Distance = 0.0f;
            if (!CharacterMesh->K2_GetClosestPointOnPhysicsAsset(Hit.ImpactPoint, SurfacePoint, Normal, BoneName, Distance))
            {
                SurfacePoint = Hit.ImpactPoint;
                BoneName = CharacterMesh->FindClosestBone(SurfacePoint);
            }
        }
    }

    // 여전히 BoneName이 None이면 false 리턴
    if (BoneName.IsNone())
    {
        return false;
    }

    OutTarget.Parent = CharacterMesh;
    OutTarget.SocketName = BoneName;
    OutTarget.SurfacePoint = SurfacePoint;

    return true;
}

void AKunaiProjectile::AttachToHitTarget(const FKunaiAttachTarget& Target, AActor* TargetActor, const FVector& IncomingDirection)
{
    SetActorRotation(IncomingDirection.Rotation(), ETeleportType::TeleportPhysics);

    USceneComponent* Parent = Target.Parent.Get();
    if (Parent && AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform, Target.SocketName))
    {
        TrackAttachedActor(TargetActor);
    }

    SetLifeSpan(FMath::Max(0.1f, AttachLifetime));

    // 표면 접촉점을 기준으로 파고든다
    const FVector WorldStart = GetActorLocation();
    const FVector WorldEnd = Target.SurfacePoint + IncomingDirection * FMath::Max(0.0f, PenetrationDepth);

    AttachMoveElapsed = 0.0f;
    if (AttachMoveDuration <= 0.0f || WorldStart.Equals(WorldEnd))
    {
        SetActorLocation(WorldEnd, false);
        return;
    }

    // 파고드는 도중에 대상이 움직여도 같은 지점으로 들어가도록 부모 기준 좌표를 저장
    const FTransform Basis = GetAttachBasis();
    AttachMoveStart = Basis.InverseTransformPosition(WorldStart);
    AttachMoveEnd = Basis.InverseTransformPosition(WorldEnd);

    // Tick에서 파고들기 수행
    SetActorTickEnabled(true);
	UNoiseSubsystem::MakeSomeNoise(this, ENoiseType::Kunai, Target.SurfacePoint, GetInstigator());
}

void AKunaiProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    AttachMoveElapsed += DeltaSeconds;
    const float Alpha = FMath::Clamp(AttachMoveElapsed / FMath::Max(KINDA_SMALL_NUMBER, AttachMoveDuration), 0.0f, 1.0f);

    SetActorLocation(GetAttachBasis().TransformPosition(FMath::Lerp(AttachMoveStart, AttachMoveEnd, Alpha)), false);

    // 파고들기가 끝나면 Tick 비활성화
    if (Alpha >= 1.0f)
    {
        SetActorTickEnabled(false);
    }
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
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Movement->StopMovementImmediately();
    Movement->Deactivate();
    
    // 스폰 시 발사 방향으로 회전하고 비행 중에는 bRotationFollowsVelocity로 속도 방향을 따르므로
    // 정면이 곧 비행 방향이다. 회전에서 구한 단위 벡터라 0이 될 수 없고, 이동 정지 순서와도 무관하다.
    const FVector IncomingDirection = GetActorForwardVector();

    // 피해 적용으로 사망 충돌이 해제되기 전에 박힐 본을 찾는다
    AttachToHitTarget(ResolveAttachTarget(OtherActor, Hit, IncomingDirection), OtherActor, IncomingDirection);

    const bool bHostile = bEnemyThrow
        ? (OtherActor->IsA<APlayerCharacter>() || OtherActor->IsA<ACloneCharacter>())
        : OtherActor->IsA<AEnemy>();
    UAbilitySystemComponent* TargetASC = bHostile
        ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor) : nullptr;

    if (SourceASC.IsValid() && DamageSpec.IsValid()
        && TargetASC && TargetASC->GetSet<UStatAttributeSet>()
        && TargetASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) > 0.0f)
    {
        // 충돌 정보와 원래 시전자 Context를 함께 전달해 적의 피격 감지/경계 전환을 유지한다.
        DamageSpec.Data->GetContext().AddHitResult(Hit, true);
        SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data.Get(), TargetASC);
    }
}
