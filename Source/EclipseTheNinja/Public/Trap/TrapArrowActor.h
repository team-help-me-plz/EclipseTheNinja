

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/ParryDeflectable.h"
#include "Interface/Poolable.h"
#include "TrapArrowActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UNiagaraComponent;

UCLASS()
class ECLIPSETHENINJA_API ATrapArrowActor : public AActor, public IPoolable, public IParryDeflectable
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATrapArrowActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnArrowBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	virtual void PostInitializeComponents() override;
	virtual void LifeSpanExpired() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void	SetLaunch(float InVelocity, float InDamage);

	// 오브젝트 풀: 발사체 이동 대상 복구 / 정지
	virtual void	OnSpawnFromPool_Implementation() override;
	virtual void	OnReturnToPool_Implementation() override;

	// 패링당하면 피해와 안전 지점 복귀 없이 반대로 날아간다.
	virtual bool	Deflect(AActor* Deflector) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<USphereComponent>		SphereCollision = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UStaticMeshComponent>	Mesh = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UProjectileMovementComponent>	ProjectileMovement = nullptr;

	// 날아가는 동안 화살에 붙어 재생할 이펙트
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UNiagaraComponent>	FlyEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Lifetime = 3.0f;

	float ArrowDamage = 0.0f;

	// 한 번 처리한 화살이 같은 이동에서 남은 오버랩으로 다시 처리되지 않도록 막는다
	bool bHasHit = false;

	// 피해 적용 중 패링으로 튕겨낸 액터. 피해 적용이 끝난 뒤 방향을 바꾼다.
	TWeakObjectPtr<AActor> PendingDeflector;

	// 튕겨낸 액터. 되돌아가는 화살이 다시 맞히지 않도록 무시한다.
	TWeakObjectPtr<AActor> DeflectedBy;

};
