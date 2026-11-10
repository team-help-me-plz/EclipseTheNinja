#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MovementLoopSoundComponent.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * 주인 캐릭터가 땅 위를 걷는 동안 루프 사운드(발소리 등)를 재생하고, 멈추면 짧게 줄이며 끈다.
 * 긴 루프 사운드를 매번 임의 지점부터 재생해, 걷는 길이만큼 잘라 쓴 것처럼 들리게 한다.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class ECLIPSETHENINJA_API UMovementLoopSoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMovementLoopSoundComponent();

	// 재생할 루프 사운드를 바꾼다. nullptr이면 이 컴포넌트는 소리를 내지 않는다
	void	SetLoopSound(USoundBase* InSound);

	// 재생 중인 소리를 바로 멈춘다 (사망·풀 반환·리셋용)
	void	StopImmediately();

	virtual void	TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void	EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 걷는 동안 반복 재생할 사운드. 에셋에서 루프를 켜 두는 것이 좋다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase>	LoopSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0"))
	float	Volume = 0.6f;

	// 이 속도(cm/s) 이상으로 땅 위를 움직일 때만 재생한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0"))
	float	MinMoveSpeed = 30.0f;

	// 걷기 시작할 때 소리가 커지는 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0"))
	float	FadeInTime = 0.1f;

	// 멈췄을 때 소리가 줄어드는 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0"))
	float	FadeOutTime = 0.25f;

	// 매번 사운드의 임의 지점부터 재생한다. 여러 몬스터가 같은 소리를 동시에 내도 박자가 겹치지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	bool	bRandomStartTime = true;

private:
	// 지금 주인이 걷고 있는지
	bool	IsOwnerWalking() const;

	void	StartLoop();
	void	StopLoop();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent>	LoopAudio = nullptr;

	bool	bIsPlaying = false;
};
