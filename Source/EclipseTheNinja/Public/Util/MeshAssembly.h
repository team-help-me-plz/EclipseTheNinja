

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MeshAssembly.generated.h"

UCLASS()
class ECLIPSETHENINJA_API AMeshAssembly : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMeshAssembly();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent>	RootScene = nullptr;

};
