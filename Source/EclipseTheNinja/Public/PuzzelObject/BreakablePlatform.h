

#pragma once

#include "CoreMinimal.h"
#include "Interface/CheckpointResettable.h"

#include "GameFramework/Actor.h"
#include "BreakablePlatform.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UCheckpointBoundComponent;

enum class EBreakablePlatformState
{
	Ready,
	Breaking,
	Broken,
};

UCLASS()
class ECLIPSETHENINJA_API ABreakablePlatform : public AActor, public ICheckpointResettable
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABreakablePlatform();

	virtual void ResetToInitial_Implementation() override;
	virtual void OnAfterReset_Implementation() override {}

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void PostInitializeComponents() override;

	UFUNCTION()
	void OnCollisionBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnCollisionEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void	BreakPlatform();
	void	TryRestore();
	void	RestorePlatform();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<USceneComponent>	RootScene = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UBoxComponent>			CollisionBox = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UChildActorComponent>	MeshAssembly = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCheckpointBoundComponent>	CheckpointBound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Break|Timer")
	float	BreakTime = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Break|Timer")
	float	RestoreTryTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Break|Timer")
	float	RestoreTime = 1.f;

private:
	FTimerHandle	PlatformBreakTimer;
	FTimerHandle	PlatformRestoreTryTimer;
	FTimerHandle	PlatformRestoreTimer;

	EBreakablePlatformState	State = EBreakablePlatformState::Ready;

	int32	OverlapCount = 0;

};
