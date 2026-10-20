

#pragma once

#include "CoreMinimal.h"
#include "Interface/Activatable.h"
#include "Interface/CheckpointResettable.h"

#include "GameFramework/Actor.h"
#include "FallingObject.generated.h"

class UCuttableComponent;
class UCheckpointBoundComponent;
class UBoxComponent;
class AMeshAssembly;

UCLASS()
class ECLIPSETHENINJA_API AFallingObject : public AActor, public IActivatable, public ICheckpointResettable
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AFallingObject();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void	ToggleActivate_Implementation() override;
	virtual void	ShowActivationIndicator_Implementation() override;
	virtual void	HideActivationIndicator_Implementation() override;

	virtual void ResetToInitial_Implementation() override;
	virtual void OnAfterReset_Implementation() override;

protected:

	UFUNCTION()
	void OnFallingBodyBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnFallingBodyEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void	FindFallingLocation();
	void	ApplyFallingDamage(AActor* InActor, const FHitResult& InHit);

	void	ObjectDrop();
	void	ObjectLand();

	// 착지 후 Pawn을 Block으로 전환한다. 아래에 Pawn이 남아 있으면 모두 빠져나갈 때까지 미룬다
	void	TryBlockPawn();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<USceneComponent>		RootScene = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Component|Rope")
	TObjectPtr<UCuttableComponent>	RopeComponent = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBoxComponent>			FallingBody = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UChildActorComponent>	FallingMeshAssembly = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UCheckpointBoundComponent>	CheckpointBoundComponent = nullptr;

	// 떨어질 위치를 보여줄 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling|Indicator")
	TObjectPtr<UStaticMeshComponent>	FallingIndicator = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling|Damage")
	float	FallingDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling|Dist")
	float	MaxTraceDistance = 1000.f;

	// 가속도 (cm/s²)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling|Move")
	float	GravityAccel = 2000.f;

	// 최대 낙하 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling|Move")
	float	MaxFallSpeed = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling|Move")
	float	InitFallSpeed = 0.f;

private:
	FVector	FallingLocation = FVector::ZeroVector;

	bool	bIsFalling = false;
	bool	bIsLanded = false;

	// 착지했지만 아래에 Pawn이 남아 있어 Block 전환을 미룬 상태
	bool	bIsPendingBlock = false;

	float	FallSpeed = 0.f;
	FVector InitialBodyRelativeLocation = FVector::ZeroVector;

	// 이번 낙하에서 이미 데미지를 받은 대상
	TSet<TWeakObjectPtr<AActor>>	HitActors;
};
