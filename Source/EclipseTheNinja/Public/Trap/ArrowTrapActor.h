

#pragma once

#include "CoreMinimal.h"
#include "Interface/Activatable.h"

#include "GameFramework/Actor.h"
#include "ArrowTrapActor.generated.h"

class UStaticMeshComponent;
class ATrapArrowActor;

UCLASS()
class ECLIPSETHENINJA_API AArrowTrapActor : public AActor, public IActivatable
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AArrowTrapActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void	ToggleActivate_Implementation() override;
	
protected:
	void	SpawnArrow();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent>			RootScene = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UChildActorComponent>	MeshAssembly = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<USceneComponent>			ArrowSpawnPos = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Trap")
	TSubclassOf<ATrapArrowActor>		ArrowClass = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Trap")
	float								ArrowSpeed = 200.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Trap")
	float								ArrowSpawnDist = 100.f;

private:
	void	ResetActivateCooldown();

	FTimerHandle	ActivateCooldownTimer;
	bool			bIsActivateCooldown = false;
	float			ActivateInterval = 0.5f;

};
