

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
	// bStartOpen이면 에디터 뷰포트에서도 열린 위치로 보여준다
	virtual void	OnConstruction(const FTransform& Transform) override;

protected:
	virtual void	BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FVector	RightDoorOpenOffset = FVector(0.f, 200.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FVector	LeftDoorOpenOffset = FVector(0.f, -200.f, 0.f);

private:
	// 미리보기로 옮긴 메시를 닫힌 위치로 되돌린다. 닫힌 위치를 기록하기 전에 먼저 호출해야 한다
	void	RestorePreviewLocation();

	// 에디터 미리보기로 메시를 연 위치에 옮겨 두었는지와 그때 쓴 오프셋. 레벨에 저장된다
	UPROPERTY()
	bool	bPreviewOpen = false;

	UPROPERTY()
	FVector	RightPreviewOffset = FVector::ZeroVector;

	UPROPERTY()
	FVector	LeftPreviewOffset = FVector::ZeroVector;

	FVector	RightDoorCloseLocation;
	FVector	LeftDoorCloseLocation;

	FVector	RightDoorOpenLocation;
	FVector	LeftDoorOpenLocation;

};
