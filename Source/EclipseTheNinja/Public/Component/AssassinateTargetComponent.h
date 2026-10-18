

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AssassinateTargetComponent.generated.h"

class UBoxComponent;
class AEnemy;
class UAssassinationTargetingComponent;
class USkeletalMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// if Actor has this component, it can be assassinate
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ECLIPSETHENINJA_API UAssassinateTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UAssassinateTargetComponent();

	UFUNCTION(BlueprintCallable, Category = "Assassinate")
	bool CanAssassinate();
	void SetAssassinationInProgress(bool bInProgress) { bAssassinationInProgress = bInProgress; }
	UFUNCTION(BlueprintCallable, Category = "Assassinate|Outline")
	void SetTargetHighlighted(bool bInValue);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION()
	void OnAssassinationBeginOverlapped(UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnAssassinationBoxEndOverlapped(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UFUNCTION()
	void AddTarget(UAssassinationTargetingComponent* ATC);

	UFUNCTION()
	void RemoveTarget(UAssassinationTargetingComponent* ATC);

protected:
	void InitializeOutlineMesh();

	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Outline")
	TSoftObjectPtr<UMaterialInterface> OutlineMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/Materials/Assassination/M_AssassinationHull.M_AssassinationHull")));

	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Outline", meta = (ClampMin = "0.0", Units = "cm"))
	float OutlineThickness = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Outline")
	FLinearColor OutlineColor = FLinearColor::Red;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> OutlineMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OutlineMaterialInstance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly ,Category = "Assassinate")
	TObjectPtr<UBoxComponent> AssassinateBoundary;

	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Boundary")
	FVector BoundaryOffset = FVector(-100.0f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Boundary", meta = (ClampMin = "0.0"))
	FVector BoundaryExtent = FVector(90.0f, 60.0f, 90.0f);

	TWeakObjectPtr<UAssassinationTargetingComponent> CurrentTargetedComp;
	bool bAssassinationInProgress = false;


};
