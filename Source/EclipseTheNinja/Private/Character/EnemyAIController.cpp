


#include "Character/EnemyAIController.h"

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!BTAsset)
	{
		UE_LOG(LogTemp, Log, TEXT("[AEnemyAIController::OnPossess] : BTAsset was nullptr"));
		return;
	}

	RunBehaviorTree(BTAsset);
}
