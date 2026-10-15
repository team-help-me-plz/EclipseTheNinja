

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

class UBehaviorTree;

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()
	
protected:
	virtual void OnPossess(APawn* InPawn) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BT")
	TObjectPtr<UBehaviorTree> BTAsset;
};
