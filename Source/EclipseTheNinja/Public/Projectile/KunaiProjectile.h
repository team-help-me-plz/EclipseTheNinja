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
 * 표창이 박힐 부모와 부모의 소켓 이름, 표면 지점을 가진 구조체
 * 부모가 없으면 월드에 고정한다.
 */
struct FKunaiAttachTarget
{
    TWeakObjectPtr<USceneComponent> Parent;
    FName SocketName = NAME_None;
    FVector SurfacePoint = FVector::ZeroVector;
};

/**
 * GAThrowKunai가 만든 피해 Spec을 보관하여 투척 애니메이션이 끝난 뒤에도 충돌 시 적용한다.
 * 오브젝트 풀에서 재사용된다. 수명 만료, 부착 대상 제거/풀 반환, 체크포인트 리스폰 시 풀로 반환된다.
 * (풀이 등록되지 않았으면 일반 스폰/파괴로 동작한다)
 */
UCLASS()
class ECLIPSETHENINJA_API AKunaiProjectile : public AActor, public IPoolable
{
    GENERATED_BODY()

public:
    AKunaiProjectile();

    // 스폰(풀에서 꺼낸) 직후 호출해 피해/발사자 정보를 설정하고 비행을 시작한다.
    void InitializeKunai(
        UAbilitySystemComponent* InSourceASC,
        const FGameplayEffectSpecHandle& InDamageSpec,
        const FVector& Direction,
        float Speed);

    float GetCollisionRadius() const;

    /** 발사체 CDO 기준으로 비행 중과 같은 충돌 채널/응답으로 발사 경로를 검사한다. */
    bool SweepLaunchPath(
        const UWorld* World,
        const FVector& Start,
        const FVector& End,
        const FCollisionQueryParams& QueryParams,
        FHitResult& OutHit) const;

    /** 손과 몸 사이에 있던 대상처럼 발사 전에 확인된 충돌을 비행 충돌과 동일하게 처리한다. */
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

    UFUNCTION()
    void OnAttachedActorDestroyed(AActor* DestroyedActor);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kunai")
    TObjectPtr<USphereComponent> Collision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kunai")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kunai")
    TObjectPtr<UProjectileMovementComponent> Movement;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai", meta = (ClampMin = "0.1", Units = "s"))
    float Lifetime = 1.0f;

    /** 표면 접촉점에서 비행 방향으로 파고들 거리. 충돌 구체 반지름과 무관하며 메시 원점에 맞춰 BP에서 조정한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Attach", meta = (ClampMin = "0.0", Units = "cm"))
    float PenetrationDepth = 8.0f;

    /** 충돌 위치에서 박힌 위치까지 이동하는 시간. 0이면 즉시 박힌 위치로 이동한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Attach", meta = (ClampMin = "0.0", Units = "s"))
    float AttachMoveDuration = 0.04f;

    /** 벽/캐릭터에 박힌 뒤 유지할 시간. 비행 중 Lifetime과 별도로 계산한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Attach", meta = (ClampMin = "0.1", Units = "s"))
    float AttachLifetime = 3.0f;

private:
    void IgnoreActorCollision(AActor* Actor);
    void IgnoreThrowerCollision();
    void IgnoreFriendlyClone(AActor* Actor);
    FKunaiAttachTarget ResolveAttachTarget(AActor* OtherActor, const FHitResult& Hit, const FVector& IncomingDirection) const;
    bool ResolveCharacterAttachTarget(USkeletalMeshComponent* CharacterMesh, const FHitResult& Hit,
                                      const FVector& IncomingDirection, FKunaiAttachTarget& OutTarget) const;
    void AttachToHitTarget(const FKunaiAttachTarget& Target, AActor* TargetActor, const FVector& IncomingDirection);
    void TrackAttachedActor(AActor* Actor);
    void OnPooledActorReturned(AActor* ReturnedActor);
    FTransform GetAttachBasis() const;

    // 이전 투척의 부착/충돌 무시/피해 정보를 모두 정리한다. 풀 반환과 EndPlay에서 공통으로 사용한다.
    void ClearThrowState();

    // 쿠나이가 박힌 벽/캐릭터. 대상이 제거되거나 풀에 반환되면 쿠나이도 함께 반환한다.
    TWeakObjectPtr<AActor> AttachedActor;
    FDelegateHandle PoolReturnHandle;

    // 발사자/아군 분신의 이동 검사에 등록한 무시 설정을 발사체 반환 시 정리한다.
    TArray<TWeakObjectPtr<UPrimitiveComponent>> IgnoringCharacterComponents;
    FDelegateHandle ActorSpawnedHandle;

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
