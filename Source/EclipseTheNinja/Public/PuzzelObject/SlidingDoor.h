

#pragma once

#include "CoreMinimal.h"
#include "PuzzelObject/BaseDoor.h"
#include "SlidingDoor.generated.h"

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API ASlidingDoor : public ABaseDoor
{
	GENERATED_BODY()
	
public:
	virtual void	DoorMovement(float DeltaTime) override;
	
protected:
	virtual void	BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FVector	RightDoorOpenOffset = FVector(0.f, 200.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FVector	LeftDoorOpenOffset = FVector(0.f, -200.f, 0.f);

private:
	FVector	RightDoorCloseLocation;
	FVector	LeftDoorCloseLocation;

	FVector	RightDoorOpenLocation;
	FVector	LeftDoorOpenLocation;

};
