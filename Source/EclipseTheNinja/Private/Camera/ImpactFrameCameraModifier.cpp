#include "Camera/ImpactFrameCameraModifier.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"

UImpactFrameCameraModifier::UImpactFrameCameraModifier()
{
	// 연출이 켜지자마자 바로 보여야 하므로 알파 블렌드 없이 적용한다.
	AlphaInTime = 0.0f;
	AlphaOutTime = 0.0f;
	Priority = 0;
}

UImpactFrameCameraModifier* UImpactFrameCameraModifier::Get(const UWorld* World)
{
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	APlayerCameraManager* CameraManager = PlayerController ? PlayerController->PlayerCameraManager.Get() : nullptr;
	if (!CameraManager)
	{
		return nullptr;
	}

	if (UCameraModifier* Existing = CameraManager->FindCameraModifierByClass(StaticClass()))
	{
		return Cast<UImpactFrameCameraModifier>(Existing);
	}

	return Cast<UImpactFrameCameraModifier>(CameraManager->AddNewCameraModifier(StaticClass()));
}

void UImpactFrameCameraModifier::PlayShake(float InDuration, float InAmplitude, float InFrequency)
{
	ShakeDuration = InDuration;
	ShakeAmplitude = InAmplitude;
	ShakeFrequency = FMath::Max(1.0f, InFrequency);
	StartRealTime = FPlatformTime::Seconds();
	LastShakeChangeTime = 0.0;
	ShakeDirection = FVector::ZeroVector;
}

bool UImpactFrameCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);

	const double Elapsed = FPlatformTime::Seconds() - StartRealTime;
	if (ShakeDuration <= 0.0f || Elapsed >= ShakeDuration)
	{
		return false;
	}

	// 정해진 주기마다 흔들림 방향을 새로 고르고, 세기는 시간이 지날수록 0까지 줄인다.
	if (Elapsed - LastShakeChangeTime >= 1.0 / ShakeFrequency || ShakeDirection.IsZero())
	{
		LastShakeChangeTime = Elapsed;
		ShakeDirection = FVector(0.0f, FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f));
	}

	const float Falloff = 1.0f - static_cast<float>(Elapsed / ShakeDuration);
	const FVector LocalOffset = ShakeDirection * ShakeAmplitude * Falloff * Alpha;

	InOutPOV.Location += InOutPOV.Rotation.RotateVector(LocalOffset);
	return false;
}
