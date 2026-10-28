#include "Subsystem/ImpactFrameSubsystem.h"

#include "Camera/ImpactFrameCameraModifier.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"

void UImpactFrameSubsystem::Deinitialize()
{
	StopAll();

	Super::Deinitialize();
}

void UImpactFrameSubsystem::PlayImpactFrame(const FImpactFrameSettings& InSettings)
{
	// 이전 히트스톱이 남아 있으면 원래 속도를 덮어쓰지 않도록 먼저 푼다.
	EndHitStop();

	Settings = InSettings;
	StartRealTime = FPlatformTime::Seconds();

	UWorld* World = GetWorld();
	AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : nullptr;
	if (WorldSettings && Settings.HitStopDuration > 0.0f)
	{
		SavedTimeDilation = WorldSettings->TimeDilation;
		WorldSettings->SetTimeDilation(Settings.HitStopTimeDilation);
		bHitStopActive = true;
	}

	if (Settings.FlashMaterial && Settings.GetEffectiveFlashDuration() > 0.0f)
	{
		SetFlashActive(true, Settings.FlashMaterial);
	}

	// 카메라 흔들림은 카메라 모디파이어가 맡는다.
	if (Settings.ShakeDuration > 0.0f)
	{
		if (UImpactFrameCameraModifier* Modifier = UImpactFrameCameraModifier::Get(World))
		{
			Modifier->PlayShake(Settings.ShakeDuration, Settings.ShakeAmplitude, Settings.ShakeFrequency);
		}
	}

	if (!TickerHandle.IsValid())
	{
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UImpactFrameSubsystem::TickImpactFrame)
		);
	}
}

bool UImpactFrameSubsystem::TickImpactFrame(float DeltaTime)
{
	const double Elapsed = FPlatformTime::Seconds() - StartRealTime;

	if (bFlashActive && Elapsed >= Settings.GetEffectiveFlashDuration())
	{
		SetFlashActive(false);
	}

	if (bHitStopActive && Elapsed >= Settings.HitStopDuration)
	{
		EndHitStop();
	}

	// 둘 다 끝나면 티커를 해제한다.
	if (bFlashActive || bHitStopActive)
	{
		return true;
	}

	TickerHandle.Reset();
	return false;
}

void UImpactFrameSubsystem::SetFlashActive(bool bActive, UMaterialInterface* Material)
{
	bFlashActive = bActive;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 레벨에 따로 두지 않도록, 화면 전체에 적용되는 볼륨을 처음 쓸 때 만든다.
	if (!IsValid(FlashVolume) && bActive)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		FlashVolume = World->SpawnActor<APostProcessVolume>(SpawnParams);
		if (IsValid(FlashVolume))
		{
			FlashVolume->bUnbound = true;
			FlashVolume->Priority = 1000.0f;
		}
	}

	if (!IsValid(FlashVolume))
	{
		return;
	}

	if (bActive && Material)
	{
		FlashVolume->Settings.WeightedBlendables.Array.Reset();
		FlashVolume->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, Material));
	}

	FlashVolume->BlendWeight = bActive ? 1.0f : 0.0f;
}

void UImpactFrameSubsystem::EndHitStop()
{
	if (!bHitStopActive)
	{
		return;
	}

	bHitStopActive = false;

	UWorld* World = GetWorld();
	AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : nullptr;
	if (WorldSettings)
	{
		WorldSettings->SetTimeDilation(SavedTimeDilation);
	}
}

void UImpactFrameSubsystem::StopAll()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	if (bFlashActive)
	{
		SetFlashActive(false);
	}

	EndHitStop();
}
