

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Cut")
	bool	bIsCutted = false;

};
