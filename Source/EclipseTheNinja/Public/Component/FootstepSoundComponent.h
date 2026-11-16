#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FootstepSoundComponent.generated.h"

class USoundAttenuation;
class USoundBase;

/**
 * 주인 캐릭터가 땅 위를 일정 거리만큼 걸을 때마다 발소리 한 번을 낸다.
 * 플레이어 발소리와 같은 방식(이동 거리 누적)이며, 감쇠 설정을 따로 줘서 먼 적의 발소리는 들리지 않게 한다.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class ECLIPSETHENINJA_API UFootstepSoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFootstepSoundComponent();

	// 발소리 후보를 바꾼다. 비우면 이 컴포넌트는 소리를 내지 않는다
	void	SetFootstepSounds(const TArray<USoundBase*>& InSounds);

	// 발소리에 쓸 감쇠 설정을 바꾼다
	void	SetAttenuation(USoundAttenuation* InAttenuation);

	// 걸은 거리를 세고, 간격을 넘으면 발소리를 낸다
	virtual void	TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction
	) override;

protected:
	// 걸음마다 이 중 하나를 무작위로 재생한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TArray<TObjectPtr<USoundBase>>	FootstepSounds;

	// 사운드 에셋의 감쇠 대신 쓸 감쇠 설정. 비우면 사운드 에셋의 설정을 따른다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundAttenuation>	Attenuation = nullptr;

	// 발소리 한 번을 내는 이동 거리(cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "1.0", Units = "cm"))
	float	StepDistance = 130.0f;

	// 플레이어 주변 범위 밖에 있을 때 발소리 한 번을 내는 이동 거리(cm). 멀리 있는 적의 발소리를 덜 자주 낸다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "1.0", Units = "cm"))
	float	OffscreenStepDistance = 150.0f;

	// 플레이어 기준 화면 안으로 볼 범위(cm). X는 가로(월드 X) 반폭, Y는 세로(월드 Z) 반높이
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0"))
	FVector2D	OnScreenRange = FVector2D(700.0f, 400.0f);

	// 발소리 음량 배율
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0"))
	float	Volume = 1.0f;

	// 이 속도(cm/s) 이상으로 땅 위를 움직일 때만 거리를 센다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0"))
	float	MinMoveSpeed = 10.0f;

private:
	// 지금 주인이 땅 위를 걷는 속도. 걷지 않으면 0
	float	GetOwnerWalkSpeed() const;

	// 주인이 플레이어 주변 범위(OnScreenRange) 안에 있는지. 플레이어가 없으면 안에 있는 것으로 본다
	bool	IsOwnerNearPlayer() const;

	// 후보 중 하나를 무작위로 골라 주인 위치에서 재생한다
	void	PlayRandomFootstep() const;

	// 마지막 발소리 뒤로 걸은 거리(cm)
	float	DistanceSinceStep = 0.0f;
};
