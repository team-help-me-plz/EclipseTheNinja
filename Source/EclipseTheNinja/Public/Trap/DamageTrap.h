

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DamageTrap.generated.h"

class UBoxComponent;

UCLASS()
class ECLIPSETHENINJA_API ADamageTrap : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ADamageTrap();

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Damage")
	float	TrapDamage = 10.f;

	// 같은 액터가 이 시간 안에 다시 겹치면 무시 (메시 본별 중복 오버랩 방지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Damage")
	float	HitCooldown = 0.5f;

	TMap<TWeakObjectPtr<AActor>, double>	LastHitTimes;

};
