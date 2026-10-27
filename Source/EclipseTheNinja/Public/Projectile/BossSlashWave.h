#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "BossSlashWave.generated.h"

class UAbilitySystemComponent;
class UBoxComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UPrimitiveComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * 보스의 원거리 발도 패턴이 날리는 검기 투사체.
 *
 * 충돌은 두 개로 나눈다.
 * - Collision(루트, 작은 구): 이동 sweep에 쓰며 벽·지형(WorldStatic)에 막히면 사라진다.
 * - HitBox(검기 크기 상자): Pawn과 겹치기만 하며, 플레이어/분신에게 한 번씩 피해를 준다.
 * 루트를 작게 두어 바닥 근처를 날아도 바닥에 걸려 사라지지 않게 한다.
 *
 * 겉모습은 BP에서 Effect(나이아가라) 또는 Mesh(스태틱 메시)에 지정한다. 둘 다 써도 된다.
 * 피해는 발사한 어빌리티가 만든 GE Spec을 그대로 적용한다.
 */
UCLASS()
class ECLIPSETHENINJA_API ABossSlashWave : public AActor
{
	GENERATED_BODY()

public:
	ABossSlashWave();

	// 스폰 직후 호출해 피해 정보와 비행 방향·속도를 설정한다.
	void InitializeSlashWave(
		UAbilitySystemComponent* InSourceASC,
		const FGameplayEffectSpecHandle& InDamageSpec,
		const FVector& Direction,
		float Speed
	);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnHitBoxOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	// 루트가 벽·지형에 막혀 이동이 멈추면 사라진다.
	UFUNCTION()
	void OnProjectileStopped(const FHitResult& ImpactResult);

	// 플레이어/분신이면 피해를 주고, 관통하지 않는 검기는 사라진다.
	void TryDamage(AActor* OtherActor, const FHitResult& Hit);

	// 소멸 이펙트를 남기고 액터를 제거한다.
	void Dissipate(const FVector& Location);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slash Wave")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slash Wave")
	TObjectPtr<UBoxComponent> HitBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slash Wave")
	TObjectPtr<UNiagaraComponent> Effect;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slash Wave")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slash Wave")
	TObjectPtr<UProjectileMovementComponent> Movement;

	/** 날아가는 최대 시간(초). 지나면 사라진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash Wave", meta = (ClampMin = "0.1", Units = "s"))
	float Lifetime = 1.5f;

	/** 플레이어를 맞힌 뒤에도 계속 날아갈지. 끄면 처음 맞힌 대상에서 사라진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash Wave")
	bool bPierceTargets = false;

	/** 벽에 막히거나 대상을 맞혀 사라질 때 그 자리에 재생할 이펙트. 비워 두면 재생하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash Wave")
	TObjectPtr<UNiagaraSystem> DissipateEffect;

private:
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
	FGameplayEffectSpecHandle DamageSpec;

	// 관통 검기가 같은 대상을 여러 번 때리지 않도록 이미 맞힌 액터를 기록한다.
	TSet<TWeakObjectPtr<AActor>> DamagedActors;

	bool bDissipated = false;
};
