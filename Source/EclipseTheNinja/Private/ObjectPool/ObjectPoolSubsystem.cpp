// Fill out your copyright notice in the Description page of Project Settings.

#include "ObjectPool/ObjectPoolSubsystem.h"
#include "ObjectPool/ObjectPoolDataAsset.h"
#include "ObjectPool/ObjectPoolSettings.h"
#include "Interface/Poolable.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace ObjectPool
{
    // 대기 중인 액터를 놓아두는 위치 (숨김 + 콜리전 꺼짐 상태)
    const FVector ParkingLocation(0.0f, 0.0f, -10000.0f);
}

void FObjectPool::RemoveInvalidActors()
{
    this->ReadyActors.RemoveAll([](const TObjectPtr<AActor>& Actor) { return !IsValid(Actor); });
    this->ActiveActors.RemoveAll([](const TObjectPtr<AActor>& Actor) { return !IsValid(Actor); });
}

bool UObjectPoolSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return (WorldType == EWorldType::Game || WorldType == EWorldType::PIE);
}

void UObjectPoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 프로젝트 세팅에 등록된 데이터 에셋으로 풀 등록
    // bWarmupInAllLevels인 풀만 미리 생성을 요청한다 (실제 생성은 월드 BeginPlay 이후)
    const UObjectPoolSettings* Settings = GetDefault<UObjectPoolSettings>();
    for (const TSoftObjectPtr<UObjectPoolDataAsset>& SoftDataAsset : Settings->PoolDataAssets)
    {
        if (SoftDataAsset.IsNull())
        {
            continue;
        }

        if (const UObjectPoolDataAsset* DataAsset = SoftDataAsset.LoadSynchronous())
        {
            this->RegisterPool(DataAsset, DataAsset->bWarmupInAllLevels);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[UObjectPoolSubsystem::Initialize()] 데이터 에셋을 불러오지 못했습니다. (%s)"),
                   *SoftDataAsset.ToString());
        }
    }
}

void UObjectPoolSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    // 서브시스템의 OnWorldBeginPlay는 액터들의 BeginPlay보다 먼저 호출되므로,
    // 모든 액터의 BeginPlay가 끝난 뒤 브로드캐스트되는 UWorld::OnWorldBeginPlay에서 미리 생성한다
    InWorld.OnWorldBeginPlay.AddUObject(this, &UObjectPoolSubsystem::HandleOnWorldBeginPlay);
}

void UObjectPoolSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        World->OnWorldBeginPlay.RemoveAll(this);
    }

    // 풀 액터들은 월드와 함께 파괴되므로 목록만 비운다
    this->Pools.Empty();
    this->bCanWarmup = false;

    Super::Deinitialize();
}

void UObjectPoolSubsystem::HandleOnWorldBeginPlay()
{
    this->bCanWarmup = true;

    TArray<TSubclassOf<AActor>> PoolClasses;
    this->Pools.GetKeys(PoolClasses);
    for (const TSubclassOf<AActor>& PoolClass : PoolClasses)
    {
        this->Warmup(PoolClass);
    }
}

bool UObjectPoolSubsystem::RegisterPool(const UObjectPoolDataAsset* InDataAsset, bool bWarmup)
{
    if (!InDataAsset || InDataAsset->ActorClass.IsNull())
    {
        return false;
    }

    const TSubclassOf<AActor> ActorClass = InDataAsset->ActorClass.LoadSynchronous();
    if (!ActorClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UObjectPoolSubsystem::RegisterPool()] %s 의 ActorClass를 불러오지 못했습니다."),
               *InDataAsset->GetName());
        return false;
    }

    if (this->Pools.Contains(ActorClass))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UObjectPoolSubsystem::RegisterPool()] %s 는 이미 풀이 등록되어 있습니다. (%s)"),
               *ActorClass->GetName(), *InDataAsset->GetName());
        return false;
    }

    FObjectPool& Pool = this->Pools.Add(ActorClass);
    Pool.InitialSize = InDataAsset->InitialSize;
    Pool.MaxSize = InDataAsset->MaxSize;
    Pool.OverflowPolicy = InDataAsset->OverflowPolicy;

    if (bWarmup)
    {
        this->RequestWarmup(InDataAsset);
    }

    return true;
}

void UObjectPoolSubsystem::RequestWarmup(const UObjectPoolDataAsset* InDataAsset, int32 InCount)
{
    if (!InDataAsset || InDataAsset->ActorClass.IsNull())
    {
        return;
    }

    const TSubclassOf<AActor> ActorClass = InDataAsset->ActorClass.LoadSynchronous();
    if (!ActorClass)
    {
        return;
    }

    // 프로젝트 세팅에 없는 레벨 전용 풀도 사용할 수 있도록 등록되지 않았으면 등록한다
    if (!this->Pools.Contains(ActorClass) && !this->RegisterPool(InDataAsset, false))
    {
        return;
    }

    this->AddWarmupRequest(ActorClass, InCount);
}

bool UObjectPoolSubsystem::RequestWarmupByClass(TSubclassOf<AActor> InClass, int32 InCount)
{
    if (!InClass || !this->Pools.Contains(InClass))
    {
        return false;
    }

    this->AddWarmupRequest(InClass, InCount);
    return true;
}

void UObjectPoolSubsystem::AddWarmupRequest(TSubclassOf<AActor> InClass, int32 InCount)
{
    FObjectPool* Pool = this->Pools.Find(InClass);
    if (!Pool)
    {
        return;
    }

    // 여러 곳(전역 설정, 레벨 설정, 스포너들)의 요청을 합산한다
    const int32 RequestedCount = (InCount < 0) ? Pool->InitialSize : InCount;
    Pool->WarmupCount += RequestedCount;

    if (this->bCanWarmup)
    {
        this->Warmup(InClass);
    }
}

void UObjectPoolSubsystem::UnregisterPool(TSubclassOf<AActor> InClass)
{
    FObjectPool Pool;
    if (!this->Pools.RemoveAndCopyValue(InClass, Pool))
    {
        return;
    }

    for (AActor* Actor : Pool.ReadyActors)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }

    for (AActor* Actor : Pool.ActiveActors)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
}

void UObjectPoolSubsystem::Warmup(TSubclassOf<AActor> InClass)
{
    FObjectPool* Pool = this->Pools.Find(InClass);
    if (!Pool)
    {
        return;
    }

    Pool->RemoveInvalidActors();

    const int32 TargetSize = (Pool->MaxSize > 0) ? FMath::Min(Pool->WarmupCount, Pool->MaxSize) : Pool->WarmupCount;
    const int32 NumToCreate = TargetSize - Pool->Num();

    for (int32 _ = 0; _ < NumToCreate; ++_)
    {
        if (!this->CreatePooledActor(InClass, FTransform(ObjectPool::ParkingLocation)))
        {
            break;
        }
    }
}

AActor* UObjectPoolSubsystem::CreatePooledActor(TSubclassOf<AActor> InClass, const FTransform& InTransform)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    // 생성 즉시 오버랩 등이 발생하지 않도록 지연 스폰으로 숨김/콜리전을 먼저 끈 뒤 스폰을 마무리한다
    AActor* Actor = World->SpawnActorDeferred<AActor>(
        InClass,
        InTransform,
        nullptr,
        nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn
    );

    if (!Actor)
    {
        return nullptr;
    }

    Actor->SetActorHiddenInGame(true);
    Actor->SetActorEnableCollision(false);
    Actor->FinishSpawning(InTransform);

    // BeginPlay 도중 스스로 파괴되었을 수 있음
    if (!IsValid(Actor))
    {
        return nullptr;
    }

    // BeginPlay에서 시작된 AI, UI 등을 액터가 직접 정리할 수 있도록 반환과 동일하게 훅을 호출한다
    if (Actor->Implements<UPoolable>())
    {
        IPoolable::Execute_OnReturnToPool(Actor);
    }

    this->DeactivateActor(Actor);

#if WITH_EDITOR
    // 아웃라이너에서 풀별로 구분되도록 ObjectPool/<클래스 이름> 폴더에 넣는다 (BP 클래스의 _C 접미사 제거)
    FString ClassName = InClass->GetName();
    ClassName.RemoveFromEnd(TEXT("_C"));
    Actor->SetFolderPath(*FString::Printf(TEXT("ObjectPool/%s"), *ClassName));
#endif

    // 해당 액터의 풀을 찾는다
    FObjectPool* Pool = this->Pools.Find(InClass);
    if (!Pool)
    {
        Actor->Destroy();
        return nullptr;
    }

    Pool->ReadyActors.Add(Actor);

    return Actor;
}

AActor* UObjectPoolSubsystem::PopReadyActor(FObjectPool& InPool)
{
    // 유효한 풀 오브젝트가 반환될 때까지 반복
    while (InPool.ReadyActors.Num() > 0)
    {
        AActor* Candidate = InPool.ReadyActors.Pop(EAllowShrinking::No);
        if (IsValid(Candidate))
        {
            return Candidate;
        }
    }

    return nullptr;
}

AActor* UObjectPoolSubsystem::SpawnActor(
    TSubclassOf<AActor> InClass,
    const FTransform& InTransform,
    AActor* InOwner,
    APawn* InInstigator)
{
    UWorld* World = GetWorld();
    if (!InClass || !World)
    {
        return nullptr;
    }

    FObjectPool* Pool = this->Pools.Find(InClass);

    // 풀에 등록되지 않은 클래스는 일반 스폰
    if (!Pool)
    {
        FActorSpawnParameters Params;
        Params.Owner = InOwner;
        Params.Instigator = InInstigator;

        return World->SpawnActor<AActor>(InClass, InTransform, Params);
    }

    Pool->RemoveInvalidActors();

    if (Pool->ReadyActors.Num() == 0)
    {
        if (!Pool->IsFull() || Pool->OverflowPolicy == EObjectPoolOverflowPolicy::Grow)
        {
            // 요청 위치에서 BeginPlay가 호출되도록 요청 트랜스폼으로 생성
            this->CreatePooledActor(InClass, InTransform);
        }
        else if (Pool->OverflowPolicy == EObjectPoolOverflowPolicy::ReuseOldest && Pool->ActiveActors.Num() > 0)
        {
            this->ReturnToPool(Pool->ActiveActors[0]);
        }
        // DoNotSpawn : 대기 액터가 없으므로 아래에서 nullptr 반환

        // 생성(BeginPlay)/반환(OnReturnToPool) 중 실행된 게임 코드가
        // Pools를 변경(RegisterPool/UnregisterPool)했을 수 있으므로
        // 풀을 다시 찾아본다
        Pool = this->Pools.Find(InClass);
        if (!Pool)
        {
            return nullptr;
        }
    }

    AActor* Actor = this->PopReadyActor(*Pool);
    if (!Actor)
    {
        return nullptr;
    }

    Actor->SetOwner(InOwner);
    Actor->SetInstigator(InInstigator);
    Actor->SetActorTransform(InTransform, false, nullptr, ETeleportType::ResetPhysics);

    // 활성화 도중(콜리전 복원 시 오버랩 등) 즉시 반환될 수 있으므로
    // 먼저 사용 중 목록에 넣은 뒤 활성화
    Pool->ActiveActors.Add(Actor);
    this->ActivateActor(Actor);

    // 생성(BeginPlay)/반환(OnReturnToPool) 중 실행된 게임 코드가
    // Pools를 변경(RegisterPool/UnregisterPool)했을 수 있으므로
    // 풀을 다시 찾아본다
    Pool = this->Pools.Find(InClass);
    if (!Pool || !Pool->ActiveActors.Contains(Actor))
    {
        return nullptr;
    }

    if (Actor->Implements<UPoolable>())
    {
        IPoolable::Execute_OnSpawnFromPool(Actor);
    }

    return Actor;
}

bool UObjectPoolSubsystem::ReturnToPool(AActor* InActor)
{
    if (!IsValid(InActor))
    {
        return false;
    }

    FObjectPool* Pool = this->Pools.Find(InActor->GetClass());
    if (!Pool)
    {
        return false;
    }

    const int32 ActiveIndex = Pool->ActiveActors.Find(InActor);
    if (ActiveIndex == INDEX_NONE)
    {
        // 이미 반환된 액터면 처리된 것으로 본다
        return Pool->ReadyActors.Contains(InActor);
    }

    // 사용 순서를 유지해야 하므로 Swap 없이 제거
    Pool->ActiveActors.RemoveAt(ActiveIndex);

    if (InActor->Implements<UPoolable>())
    {
        IPoolable::Execute_OnReturnToPool(InActor);
    }

    this->DeactivateActor(InActor);

    if (FObjectPool* CurrentPool = this->Pools.Find(InActor->GetClass()))
    {
        CurrentPool->ReadyActors.Add(InActor);
    }

    return true;
}

void UObjectPoolSubsystem::ReleaseActor(AActor* InActor)
{
    if (!IsValid(InActor))
    {
        return;
    }

    if (!this->ReturnToPool(InActor))
    {
        InActor->Destroy();
    }
}

void UObjectPoolSubsystem::Release(AActor* InActor)
{
    if (!IsValid(InActor))
    {
        return;
    }

    UWorld* World = InActor->GetWorld();
    if (UObjectPoolSubsystem* Subsystem = World ? World->GetSubsystem<UObjectPoolSubsystem>() : nullptr)
    {
        Subsystem->ReleaseActor(InActor);
        return;
    }

    InActor->Destroy();
}

void UObjectPoolSubsystem::ActivateActor(AActor* InActor)
{
    const AActor* DefaultActor = InActor->GetClass()->GetDefaultObject<AActor>();

    InActor->SetActorHiddenInGame(DefaultActor->IsHidden());
    InActor->SetActorEnableCollision(DefaultActor->GetActorEnableCollision());
    InActor->SetActorTickEnabled(InActor->PrimaryActorTick.bStartWithTickEnabled);

    // 새로 스폰될 때와 같은 규칙: 자동 활성 컴포넌트는 Activate(틱 켜짐), 나머지는 bStartWithTickEnabled를 따름
    TInlineComponentArray<UActorComponent*> Components(InActor);
    for (UActorComponent* Component : Components)
    {
        if (Component->bAutoActivate)
        {
            Component->Activate(true);
        }
        else
        {
            Component->SetComponentTickEnabled(Component->PrimaryComponentTick.bStartWithTickEnabled);
        }
    }
}

void UObjectPoolSubsystem::DeactivateActor(AActor* InActor)
{
    InActor->SetActorHiddenInGame(true);
    InActor->SetActorEnableCollision(false);
    InActor->SetActorTickEnabled(false);

    if (UWorld* World = InActor->GetWorld())
    {
        World->GetTimerManager().ClearAllTimersForObject(InActor);
    }

    TInlineComponentArray<UActorComponent*> Components(InActor);
    for (UActorComponent* Component : Components)
    {
        if (Component->IsActive())
        {
            Component->Deactivate();
        }
        Component->SetComponentTickEnabled(false);

        // 물리 시뮬레이션 중지
        if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
        {
            if (Primitive->IsSimulatingPhysics())
            {
                Primitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
                Primitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            }
        }
    }

    InActor->SetActorLocation(ObjectPool::ParkingLocation, false, nullptr, ETeleportType::ResetPhysics);
}
