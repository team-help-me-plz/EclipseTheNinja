#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "ImpactFrameCameraModifier.generated.h"

/**
 * 임팩트 프레임의 카메라 흔들림을 맡는 카메라 모디파이어. 화면 효과와 히트스톱은 UImpactFrameSubsystem이 맡는다.
 * 시간은 실제 시간으로 재므로 히트스톱 중에도 정해진 시간에 끝난다.
 * 카메라 매니저가 UpdateViewTarget에서 ApplyCameraModifiers를 호출해야 적용된다.
 */
UCLASS()
class ECLIPSETHENINJA_API UImpactFrameCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	UImpactFrameCameraModifier();

	/** 월드의 첫 번째 로컬 플레이어 카메라에 붙은 모디파이어를 찾고, 없으면 추가한다. 카메라가 없으면 nullptr. */
	static UImpactFrameCameraModifier* Get(const UWorld* World);

	/** 흔들림을 시작한다. 이미 흔들리는 중이면 새 값으로 다시 시작한다. */
	void PlayShake(float InDuration, float InAmplitude, float InFrequency);

	// 화면 기준 좌우·위아래로 흔들림 위치를 더한다.
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

protected:
	float ShakeDuration = 0.0f;
	float ShakeAmplitude = 0.0f;
	float ShakeFrequency = 30.0f;

	// 흔들림을 시작한 실제 시간(초)
	double StartRealTime = 0.0;

	// 흔들림 방향을 바꾼 시간(시작 기준 초)과 지금 흔들림 방향
	double LastShakeChangeTime = 0.0;
	FVector ShakeDirection = FVector::ZeroVector;
};
