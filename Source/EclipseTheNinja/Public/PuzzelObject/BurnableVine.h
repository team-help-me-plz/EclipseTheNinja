

#pragma once

#include "CoreMinimal.h"
#include "Interface/CheckpointResettable.h"

#include "GameFramework/Actor.h"
#include "BurnableVine.generated.h"

class UBoxComponent;
class UChildActorComponent;
class UCheckpointBoundComponent;
class UNiagaraComponent;

UCLASS()
class ECLIPSETHENINJA_API ABurnableVine : public AActor, public ICheckpointResettable
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABurnableVine();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	virtual void ResetToInitial_Implementation() override;
	virtual void OnAfterReset_Implementation() override;

protected:
	// 불에 탈 때 실행할 함수 
	void	Ignite();
	
	// 불에 탄 후 불려올 함수.
	void	FinishBurn();
	
	// 불이 붙었을 때 주변에 불을 옮기는 함수.
	void	SpreadFire();

	// MeshAssembly(Child Actor)를 보이거나 숨긴다. Child Actor Class가 비어 있으면 아무것도 하지 않는다.
	void	SetAssemblyVisible(UChildActorComponent* InAssembly, bool bVisible);

	// MeshAssembly 안 메시들의 충돌을 끈다. 충돌은 CollisionBox 하나로만 처리한다.
	void	DisableAssemblyCollision(UChildActorComponent* InAssembly);

	// 시작 시 CollisionBox와 겹쳐 있는 다른 Vine을 NeighborVines에 추가한다. (에디터에서 지정한 Vine은 그대로 유지)
	void	CollectOverlappingVines();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<USceneComponent>	RootScene = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBoxComponent>	CollisionBox = nullptr;

	// 타기 전 외형. Child Actor Class에 AMeshAssembly를 상속한 BP를 지정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UChildActorComponent>	IdleMeshAssembly = nullptr;

	// 다 탄 뒤 외형. 비워 두면 다 탔을 때 아무것도 남지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UChildActorComponent>	BurnedMeshAssembly = nullptr;

	// 불이 붙어 있는 동안 재생할 불꽃 이펙트. BP에서 Niagara System Asset을 지정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UNiagaraComponent>	BurnEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UCheckpointBoundComponent>	CheckpointBoundComponent = nullptr;

	// 불이 붙었을 때 불타 없어지는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Burn", meta = (ClampMin = "0.01"))
	float	BurnDuration = 1.0f;

	// 불이 붙었을 때 주변 Vine으로 옮겨 붙는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Burn", meta = (ClampMin = "0.01"))
	float	SpreadDelay = 0.5f;

	// 주변에 연결된 Vine. 이 Vine이 불에 탈 때 불이 없는 주변 Vine으로 불이 옮겨 붙는다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Burn")
	TArray<TObjectPtr<ABurnableVine>>	NeighborVines;

	// 불이 붙었는지 확인하는 값
	UPROPERTY(VisibleAnywhere, Category = "Burn")
	bool	bIsBurning = false;

	// 불에 탔는지 확인하는 값
	UPROPERTY(VisibleAnywhere, Category = "Burn")
	bool	bIsBurned = false;

	// 길을 막는 Vine으로 설정하는 값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vine")
	bool	bIsBlockPath = true;

	// 상호작용을 막을 대상
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Vine|Lock")
	TObjectPtr<AActor>	TargetActor = nullptr;

private:
	// 불에 다 탄 것을 연출할 타이머
	FTimerHandle	BurnTimer;
	// 주변 Vine에 불을 옮겨 붙일 타이머
	FTimerHandle	SpreadTimer;

};
