// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ObjectPool/ObjectPoolTypes.h"
#include "ObjectPoolSubsystem.generated.h"

class UObjectPoolDataAsset;

// 클래스 하나에 대한 풀
USTRUCT()
struct FObjectPool
{
    GENERATED_BODY()

    // 사용 대기 중인 액터들
    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> ReadyActors;

    // 사용 중인 액터들 (앞쪽일수록 오래 전에 스폰됨)
    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> ActiveActors;

    int32 InitialSize = 0;
    int32 MaxSize = 0;

    // 이 월드에서 미리 생성할 수량. bWarmupInAllLevels, 레벨의 AObjectPoolLevelConfig, 스포너들의 요청 수량을 합산한 값
    int32 WarmupCount = 0;
    EObjectPoolOverflowPolicy OverflowPolicy = EObjectPoolOverflowPolicy::Grow;

    int32 Num() const { return this->ReadyActors.Num() + this->ActiveActors.Num(); }
    bool IsFull() const { return this->MaxSize > 0 && this->Num() >= this->MaxSize; }

    // 외부에서 Destroy된 액터들을 목록에서 제거
    void RemoveInvalidActors();

};

/**
 * 월드 단위 오브젝트 풀 서브시스템.
 *  - 등록 : UObjectPoolSettings(Project Settings)의 데이터 에셋들을 월드 초기화 시 등록 (등록만으로는 액터를 만들지 않는다)
 *  - 미리 생성 : bWarmupInAllLevels, 레벨에 배치된 AObjectPoolLevelConfig, 스포너(RequestWarmupByClass)의 요청 수량을 합산해
 *               월드 BeginPlay 후 생성. 요청이 없는 풀은 처음 스폰될 때 생성된다
 *  - 스폰 : SpawnActor로 요청. 풀이 없는 클래스는 일반 SpawnActor로 처리되므로 호출부는 풀 등록 여부를 신경 쓰지 않아도 된다
 *  - 반환 : ReleaseActor(또는 static Release)로 반환. 풀 액터가 아니면 파괴된다
 * 풀 액터는 레벨 전환 시 월드와 함께 파괴되며, 다음 월드에서 새로 만들어진다.
 */
UCLASS()
class ECLIPSETHENINJA_API UObjectPoolSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

    // 데이터 에셋으로 풀을 등록하는 함수. 이미 등록된 클래스면 실패한다
    // bWarmup : InitialSize만큼 미리 생성 요청 (RequestWarmup과 동일)
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    bool RegisterPool(const UObjectPoolDataAsset* InDataAsset, bool bWarmup = true);

    // 데이터 에셋으로 미리 생성을 요청하는 함수. 등록되지 않은 풀이면 먼저 등록한다
    // InCount가 음수면 InitialSize를 사용하며, 여러 번 요청되면 수량을 합산한다
    // 월드 BeginPlay 이전이면 BeginPlay 후에, 이후라면 즉시 생성한다
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    void RequestWarmup(const UObjectPoolDataAsset* InDataAsset, int32 InCount = -1);

    // 클래스로 미리 생성을 요청하는 함수 (스포너가 자신이 스폰할 클래스를 요청할 때 사용)
    // 풀이 등록된 클래스만 적용되며, 등록되지 않았으면 false. 수량 규칙은 RequestWarmup과 같다
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    bool RequestWarmupByClass(TSubclassOf<AActor> InClass, int32 InCount = -1);

    // 풀을 제거하는 함수. 풀이 관리하던 액터는 사용 중인 것까지 모두 파괴된다
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    void UnregisterPool(TSubclassOf<AActor> InClass);

    UFUNCTION(BlueprintPure, Category = "ObjectPool")
    bool HasPool(TSubclassOf<AActor> InClass) const { return this->Pools.Contains(InClass); }

    // 액터를 스폰하는 함수. 풀이 등록된 클래스면 풀에서 꺼내고, 아니면 일반 스폰한다
    // 풀이 가득 찼고 정책이 DoNotSpawn이면 nullptr를 반환한다
    UFUNCTION(BlueprintCallable, Category = "ObjectPool", meta = (DeterminesOutputType = "InClass"))
    AActor* SpawnActor(
        TSubclassOf<AActor> InClass,
        const FTransform& InTransform,
        AActor* InOwner = nullptr,
        APawn* InInstigator = nullptr);

    template<typename T>
    T* SpawnActor(
        TSubclassOf<T> InClass,
        const FTransform& InTransform,
        AActor* InOwner = nullptr,
        APawn* InInstigator = nullptr)
    {
        return Cast<T>(this->SpawnActor(TSubclassOf<AActor>(InClass), InTransform, InOwner, InInstigator));
    }

    // 풀 액터를 풀에 반환하는 함수. 풀 액터가 아니면 false (이미 대기 중인 액터는 true)
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    bool ReturnToPool(AActor* InActor);

    // 풀 액터는 반환하고, 그 외의 액터는 파괴하는 함수 (Destroy() 대신 사용)
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    void ReleaseActor(AActor* InActor);

    // 액터가 속한 월드의 서브시스템으로 ReleaseActor를 호출하는 헬퍼. 서브시스템이 없으면 Destroy한다
    static void Release(AActor* InActor);

protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
    // 액터들의 BeginPlay가 끝난 뒤(UWorld::OnWorldBeginPlay) 모든 풀을 미리 생성
    void HandleOnWorldBeginPlay();

    // 요청 수량을 WarmupCount에 더하고, 월드 BeginPlay 이후라면 즉시 생성하는 함수
    void AddWarmupRequest(TSubclassOf<AActor> InClass, int32 InCount);

    // WarmupCount가 될 때까지 대기 액터를 생성하는 함수
    void Warmup(TSubclassOf<AActor> InClass);

    // 비활성 상태의 액터를 새로 만들어 ReadyActors에 추가하는 함수 (OnReturnToPool도 호출된다)
    AActor* CreatePooledActor(TSubclassOf<AActor> InClass, const FTransform& InTransform);

    // 풀에서 유효한 풀 오브젝트를 반환하는 함수
    AActor* PopReadyActor(FObjectPool& InPool);

    // 숨김/콜리전/틱/컴포넌트 활성 상태를 클래스 기본값으로 복원
    void ActivateActor(AActor* InActor);

    // 숨김, 콜리전/틱 끄기, 컴포넌트 Deactivate, 타이머 정리, 대기 위치로 이동
    void DeactivateActor(AActor* InActor);

private:
    UPROPERTY(Transient)
    TMap<TSubclassOf<AActor>, FObjectPool> Pools;

    // 월드 BeginPlay 이후에만 Warmup 가능하도록 하는 플래그
    bool bCanWarmup = false;

};
