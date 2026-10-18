

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AssassinationTargetingComponent.generated.h"

class AEnemy;
class UAssassinateTargetComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ECLIPSETHENINJA_API UAssassinationTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UAssassinationTargetingComponent();

	UAssassinateTargetComponent* GetCurrentTarget() const { return CurrentTarget; }
	void AddTargetCandidate(UAssassinateTargetComponent* Enemy);
	void RemoveTargetCandidate(UAssassinateTargetComponent* Enemy);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void SelectCandidate();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Assassination")
	float DetectRange = 100.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Assassination")
	TSet<UAssassinateTargetComponent*> TargetCandidate;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Assassination")
	TObjectPtr<UAssassinateTargetComponent> CurrentTarget;

private:
	FTimerHandle  SearchCandidateTimer;



};
