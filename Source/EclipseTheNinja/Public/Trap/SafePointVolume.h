

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SafePointVolume.generated.h"

class UBoxComponent;
class ATargetPoint;

UCLASS()
class ECLIPSETHENINJA_API ASafePointVolume : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASafePointVolume();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

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

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent>	RootScene = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UBoxComponent>	CollisionBox = nullptr;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "SafePoint")
	TObjectPtr<ATargetPoint>	SafePoint = nullptr;

};
