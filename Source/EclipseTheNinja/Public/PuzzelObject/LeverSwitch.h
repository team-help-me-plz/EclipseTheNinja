

#pragma once

#include "CoreMinimal.h"
#include "PuzzelObject/InteractSwitch.h"
#include "LeverSwitch.generated.h"

class UBoxComponent;

/**
 *
 */
UCLASS()
class ECLIPSETHENINJA_API ALeverSwitch : public AInteractSwitch
{
	GENERATED_BODY()

public:
	ALeverSwitch();

	virtual void	Interact_Implementation(AActor* InActor) override;
	virtual void	ResetToInitial_Implementation() override;

protected:
	virtual void	BeginPlay() override;

	virtual void	ResetInteractionCooldown() override;

	void	PlayLeverAnim();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent>	LeverMesh = nullptr;

	// 상호작용 감지(InteractBox 오버랩, 분신 조준 Visibility 트레이스)용 판정. 손잡이 회전과 무관하게 고정
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent>			InteractCollision = nullptr;

	// Off -> On 방향 시퀀스. On -> Off는 역재생
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lever")
	TObjectPtr<UAnimSequence>			LeverSequence = nullptr;

private:
	bool	bIsToggled = false;
	float	SequenceLength = 0.f;
	float	PlayRate = 1.f;

};
