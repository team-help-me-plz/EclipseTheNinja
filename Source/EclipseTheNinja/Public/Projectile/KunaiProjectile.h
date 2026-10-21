#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "Interface/Poolable.h"
#include "KunaiProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UAbilitySystemComponent;
class UPrimitiveComponent;
class USkeletalMeshComponent;

/**
 * 표창이 박힐 부모, 부모의 소켓 이름, 표면 지점을 가진 구조체
 * 부모가 없으면 월드에 고정한다.
 */
struct FKunaiAttachTarget
{
    TWeakObjectPtr<USceneComponent> Parent;
    FName SocketName = NAME_None;
    FVector SurfacePoint = FVector::ZeroVector;
};

/**
 * GAThrowKunai가 만든 피해 Spec을 보관하여 충돌 시 적용한다.
 * 오브젝트 풀에서 재사용된다. 수명 만료, 부착 대상 제거/풀 반환, 체크포인트 리스폰 시 풀로 반환된다.
 * (풀이 등록되지 않았으면 일반 스폰/파괴로 동작한다)
 */
UCLASS()
class ECLIPSETHENINJA_API AKunaiProjectile : public AActor, public IPoolable
{
    GENERATED_BODY()

public:
    AKunaiProjectile();

    // 스폰 직후 호출해 피해/발사자 정보를 설정하고 날아가도록 하는 함수
    void InitializeKunai(
        UAbilitySystemComponent* InSourceASC,
        const FGameplayEffectSpecHandle& InDamageSpec,
        const FVector& Direction,
        float Speed);

    float GetCollisionRadius() const;

    // 투척 전 캐릭터 중심에서 발사 위치(손)까지의 충돌을 검사하는 함수
    // 비행 중과 같은 기준이 되도록 발사체 Collision의 채널과 응답을 그대로 사용한다 (CDO에서 호출)
    bool SweepLaunchPath(
        const UWorld* World,
        const FVector& Start,
        const FVector& End,
        const FCollisionQueryParams& QueryParams,
        FHitResult& OutHit) const;

    // 투척 전에 확인된 충돌을 비행 충돌과 동일하게 처리하는 함수
    void HandleLaunchHit(const FHitResult& Hit);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void LifeSpanExpired() override;

    virtual void OnReturnToPool_Implementation() override;

    UFUNCTION()
    void OnProjectileHit(
        UPrimitiveComponent* HitComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        FVector NormalImpulse,
        const FHitResult& Hit);

    // 부착된 액터가 파괴되었을 때 발사체를 풀로 되돌리는 함수
    UFUNCTION()
    void OnAttachedActorDestroyed(AActor* DestroyedActor);

private:
    /** InActor의 컴포넌트들을 모두 무시하도록 설정한 뒤 IgnoringCharacterComponents에 추가하는 함수 */
    void IgnoreActorCollision(AActor* InActor);

    /** 발사체의 Owner, Instigator의 컴포넌트들을 모두 무시하도록 설정한 뒤 IgnoringCharacterComponents에 추가하는 함수 */
    void IgnoreThrowerCollision();

    /** 플레이어가 투척한 표창일 경우, 플레이어가 소환한 분신의 컴포넌트들을 모두 무시하도록 설정한 뒤 IgnoringCharacterComponents에 추가하는 함수 */
    void IgnoreFriendlyClone(AActor* InClone);

    /** 충돌한 액터를 기준으로 표창이 박힐 위치에 대한 정보인 FKunaiAttachTarget구조체를 생성해 반환하는 함수 */
    FKunaiAttachTarget ResolveAttachTarget(AActor* OtherActor, const FHitResult& Hit, const FVector& IncomingDirection) const;

    /** 캐릭터 메시에서 표창이 박힐 본과 표면 지점을 찾는 함수. 본을 찾지 못하면 false */
    bool ResolveCharacterAttachTarget(
        USkeletalMeshComponent* CharacterMesh,
        const FHitResult& Hit,
        const FVector& IncomingDirection,
        FKunaiAttachTarget& OutTarget) const;
    
    /** 타겟에 부착한 뒤 Tick을 활성화해 파고들기를 시작하는 함수 */
    void AttachToHitTarget(const FKunaiAttachTarget& Target, AActor* TargetActor, const FVector& IncomingDirection);

    /** 표창이 박힌 액터를 갱신하고, 해당 액터가 파괴 혹은 반환될 시, 표창도 풀에 반환되도록 바인딩하는 함수 */
    void TrackAttachedActor(AActor* InActor);

    /** 오브젝트 풀의 반환 이벤트에 바인딩할 함수. */ 
    void OnPooledActorReturned(AActor* ReturnedActor);

    /** 파고들기 위치 계산의 기준 트랜스폼. 부착된 부모 소켓의 트랜스폼, 부착되지 않았으면 월드(Identity) */
    FTransform GetAttachBasis() const;

    // 이전 투척의 부착/충돌 무시/피해 정보를 모두 정리한다. 풀 반환과 EndPlay에서 공통으로 사용한다.
    void ClearThrowState();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kunai")
    TObjectPtr<USphereComponent> Collision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kunai")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kunai")
    TObjectPtr<UProjectileMovementComponent> Movement;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai", meta = (ClampMin = "0.1", Units = "s"))
    float Lifetime = 1.0f;

    // 표면 접촉점에서 비행 방향으로 파고들 거리. 충돌 구체 반지름과 무관하며 메시 원점에 맞춰 BP에서 조정하도록 함
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Attach", meta = (ClampMin = "0.0", Units = "cm"))
    float PenetrationDepth = 8.0f;

    // 충돌 위치에서 박힌 위치까지 이동하는 시간 (0이면 즉시 박힌 위치로 이동)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Attach", meta = (ClampMin = "0.0", Units = "s"))
    float AttachMoveDuration = 0.04f;

    // 벽/캐릭터에 박힌 뒤 유지할 시간. 비행 중 Lifetime과 별도로 계산됨
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Attach", meta = (ClampMin = "0.1", Units = "s"))
    float AttachLifetime = 3.0f;

private:
    // 표창이 박힌 액터
    TWeakObjectPtr<AActor> AttachedActor;

    // 발사체가 충돌을 무시할 컴포넌트 목록
    TArray<TWeakObjectPtr<UPrimitiveComponent>> IgnoringCharacterComponents;
    
    FDelegateHandle ActorSpawnedHandle;
    FDelegateHandle PoolReturnHandle;

    UPROPERTY(Transient)
    TWeakObjectPtr<UAbilitySystemComponent> SourceASC;

    UPROPERTY(Transient)
    FGameplayEffectSpecHandle DamageSpec;

    FVector LaunchVelocity = FVector::ZeroVector;
    bool bEnemyThrow = false;
    bool bPlayerThrow = false;
    bool bConsumed = false;

    // 부착한 부모 소켓 기준 로컬 좌표. 부모가 없으면 월드 좌표
    FVector AttachMoveStart = FVector::ZeroVector;
    FVector AttachMoveEnd = FVector::ZeroVector;
    float AttachMoveElapsed = 0.0f;

};
