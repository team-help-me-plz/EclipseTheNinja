#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "KunaiProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UAbilitySystemComponent;
class UPrimitiveComponent;
class AEnemy;

/** GAThrowKunai가 만든 피해 Spec을 보관하여 투척 애니메이션이 끝난 뒤에도 충돌 시 적용한다. */
UCLASS()
class ECLIPSETHENINJA_API AKunaiProjectile : public AActor
{
    GENERATED_BODY()

public:
    AKunaiProjectile();

    // Deferred Spawn과 FinishSpawning 사이에서 호출하는 초기화 함수
    void InitializeKunai(
        UAbilitySystemComponent* InSourceASC,
        const FGameplayEffectSpecHandle& InDamageSpec,
        const FVector& Direction,
        float Speed);

    float GetCollisionRadius() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION()
    void OnProjectileHit(
        UPrimitiveComponent* HitComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        FVector NormalImpulse,
        const FHitResult& Hit);

    UFUNCTION()
    void OnEmbeddedEnemyDestroyed(AActor* DestroyedActor);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kunai")
    TObjectPtr<USphereComponent> Collision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kunai")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kunai")
    TObjectPtr<UProjectileMovementComponent> Movement;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai", meta = (ClampMin = "0.1", Units = "s"))
    float Lifetime = 1.0f;

    // 벽 충돌 직후 위치에서 비행 방향으로 더 진행할 거리
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Wall Impact", meta = (ClampMin = "0.0", Units = "cm"))
    float WallEmbedDistance = 24.0f;

    // 추가 진행에 걸리는 시간. 0이면 즉시 박힌 위치로 이동한다
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Wall Impact", meta = (ClampMin = "0.0", Units = "s"))
    float WallEmbedDuration = 0.04f;

    // 벽에 박혀 있는 시간
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Wall Impact", meta = (ClampMin = "0.0", Units = "s"))
    float WallEmbedLifetime = 2.0f;

    /** 몬스터에 박힌 뒤 유지할 시간. 비행 중 Lifetime과 별도로 계산한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Enemy Impact", meta = (ClampMin = "0.1", Units = "s"))
    float EnemyEmbedLifetime = 3.0f;

    /** 메시 표면에서 비행 방향으로 파고들 거리. 메시 원점에 맞춰 BP에서 조정한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kunai|Enemy Impact", meta = (ClampMin = "0.0", Units = "cm"))
    float EnemyEmbedDistance = 8.0f;

private:
    void IgnoreActorCollision(AActor* Actor);
    void IgnoreThrowerCollision();
    void IgnoreFriendlyClone(AActor* Actor);
    void BeginWallEmbedding(const FVector& IncomingDirection);
    void EmbedInEnemy(AEnemy* Enemy, const FHitResult& Hit, const FVector& IncomingDirection);

    TWeakObjectPtr<AEnemy> EmbeddedEnemy;

    // 발사자/아군 분신의 이동 검사에 등록한 무시 설정을 발사체 제거 시 정리한다.
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

    FVector WallEmbedStart = FVector::ZeroVector;
    FVector WallEmbedEnd = FVector::ZeroVector;
    float WallEmbedElapsed = 0.0f;

};
