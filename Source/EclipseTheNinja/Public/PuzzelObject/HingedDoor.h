

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
	// bStartOpen이면 에디터 뷰포트에서도 열린 각도로 보여준다
	virtual void	OnConstruction(const FTransform& Transform) override;

protected:
	virtual void	BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FRotator	RightDoorOpenOffset = FRotator(0.f, 90.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FRotator	LeftDoorOpenOffset = FRotator(0.f, -90.f, 0.f);

private:
	// 에디터 미리보기로 메시를 연 각도로 돌려 두었는지. 레벨에 저장된다
	UPROPERTY()
	bool	bPreviewOpen = false;

};
