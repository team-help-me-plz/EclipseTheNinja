

#pragma once

#include "CoreMinimal.h"
#include "PuzzelObject/BaseDoor.h"
#include "HingedDoor.generated.h"

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API AHingedDoor : public ABaseDoor
{
	GENERATED_BODY()
	
public:
	virtual void	DoorMovement(float DeltaTime) override;

protected:
	virtual void	BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FRotator	RightDoorOpenOffset = FRotator(0.f, 90.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FRotator	LeftDoorOpenOffset = FRotator(0.f, -90.f, 0.f);

};
