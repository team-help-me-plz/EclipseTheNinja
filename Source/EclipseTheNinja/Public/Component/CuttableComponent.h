

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "CuttableComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnRopeCut);

/**
 *
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ECLIPSETHENINJA_API UCuttableComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UCuttableComponent();

	void	CutRope();
	void	RestoreRope();
	void	SetRopeLength(const float InLength);

	FOnRopeCut	OnRopeCut;

protected:
	virtual void	BeginPlay() override;

	// 표창은 오버랩 이벤트를 내지 않고 Block으로 맞으므로 Hit으로 받는다
	UFUNCTION()
	void	OnRopeHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	// 줄에 박혀 있던 표창 등을 풀로 돌려보낸다 (줄은 숨기기만 하므로 그대로 두면 허공에 떠 있게 된다)
	void	ReleaseAttachedActors();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Cut")
	bool	bIsCutted = false;

};
