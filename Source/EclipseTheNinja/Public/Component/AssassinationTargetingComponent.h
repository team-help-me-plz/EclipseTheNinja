

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AssassinationTargetingComponent.generated.h"

class AEnemy;
class UAssassinateTargetComponent;

/**
 * 현재 타겟에 실행할 암살 방식. 적과 같은 바닥이면 평지, 적보다 높은 발판 위면 공중(뛰어내리기),
 * 적이 선 발판보다 낮은 바닥이면 아래(모서리를 잡고 올라가기)다.
 */
UENUM(BlueprintType)
enum class EAssassinationType : uint8
{
	None,
	Ground,
	Aerial,
	Below,
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ECLIPSETHENINJA_API UAssassinationTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UAssassinationTargetingComponent();

	UAssassinateTargetComponent* GetCurrentTarget() const { return CurrentTarget; }
	EAssassinationType GetCurrentTargetType() const { return CurrentTarget ? CurrentTargetType : EAssassinationType::None; }

	void AddTargetCandidate(UAssassinateTargetComponent* Enemy);
	void RemoveTargetCandidate(UAssassinateTargetComponent* Enemy);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// 후보중에서 타겟을 선정하는 함수
	void SelectCandidate();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Assassination")
	float DetectRange = 100.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Assassination")
	TSet<UAssassinateTargetComponent*> TargetCandidate;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Assassination")
	TObjectPtr<UAssassinateTargetComponent> CurrentTarget;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Assassination")
	EAssassinationType CurrentTargetType = EAssassinationType::None;

private:
	FTimerHandle  SearchCandidateTimer;



};
