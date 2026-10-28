#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "ImpactFrameSubsystem.generated.h"

class APostProcessVolume;
class UMaterialInterface;

/**
 * 임팩트 프레임 연출 한 번의 설정. 모든 시간은 실제 시간(초)이라 히트스톱으로 게임이 느려져도 늘어나지 않는다.
 */
USTRUCT(BlueprintType)
struct ECLIPSETHENINJA_API FImpactFrameSettings
{
	GENERATED_BODY()

	/** 화면을 덮을 포스트 프로세스 머티리얼(2톤, 반전 등). 비우면 화면 효과 없이 흔들림과 히트스톱만 준다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Frame|Flash")
	TObjectPtr<UMaterialInterface> FlashMaterial;

	/**
	 * 화면 효과를 히트스톱이 걸려 있는 동안만 보여 준다. 켜 두면 FlashDuration 대신 HitStopDuration을 쓴다.
	 * 히트스톱을 쓰지 않으면(HitStopDuration 0) FlashDuration을 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Frame|Flash")
	bool bFlashDuringHitStop = true;

	/** 화면 효과를 켜 둘 시간(초). 0.05~0.1초 정도가 애니메이션의 임팩트 프레임 느낌이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Frame|Flash", meta = (ClampMin = "0.0", Units = "s"))
	float FlashDuration = 0.08f;

	/** 실제로 화면 효과를 켜 둘 시간(초). */
	float GetEffectiveFlashDuration() const
	{
		return bFlashDuringHitStop && HitStopDuration > 0.0f
			? HitStopDuration
			: FlashDuration;
	}

	/** 게임을 멈춘 것처럼 느리게 할 시간(초). 0이면 히트스톱을 쓰지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Frame|Hit Stop", meta = (ClampMin = "0.0", Units = "s"))
	float HitStopDuration = 0.08f;

	/** 히트스톱 동안의 게임 속도 배율. 0.05면 거의 멈춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Frame|Hit Stop", meta = (ClampMin = "0.0001", ClampMax = "1.0"))
	float HitStopTimeDilation = 0.05f;

	/**
	 * 카메라가 흔들리는 시간(초). 흔들림 세기는 이 시간 동안 0까지 줄어든다. 0이면 흔들지 않는다.
	 * 카메라 모디파이어로 흔들기 때문에, 카메라 매니저가 UpdateViewTarget에서 ApplyCameraModifiers를 호출해야 보인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Frame|Shake", meta = (ClampMin = "0.0", Units = "s"))
	float ShakeDuration = 0.25f;

	/** 카메라 흔들림의 최대 이동 거리(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Frame|Shake", meta = (ClampMin = "0.0", Units = "cm"))
	float ShakeAmplitude = 12.0f;

	/** 1초에 흔들림 방향을 바꾸는 횟수. 높을수록 잘게 떨린다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Frame|Shake", meta = (ClampMin = "1.0", Units = "Hz"))
	float ShakeFrequency = 30.0f;
};

/**
 * 애니메이션의 임팩트 프레임처럼 화면을 순간 2톤으로 바꾸고 히트스톱을 거는 월드 서브시스템.
 * 화면 효과는 범위 제한 없는 임시 포스트 프로세스 볼륨으로, 히트스톱은 월드 타임 딜레이션으로 준다.
 * 끝나는 시점은 실제 시간 티커로 재므로 카메라 매니저 구현이나 게임 속도와 상관없이 정해진 시간에 풀린다.
 */
UCLASS()
class ECLIPSETHENINJA_API UImpactFrameSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** 임팩트 프레임을 시작한다. 이미 재생 중이면 새 설정으로 다시 시작한다. */
	void PlayImpactFrame(const FImpactFrameSettings& InSettings);

protected:
	// 게임/PIE 월드에서만 만든다. 에디터 프리뷰 월드(애니메이션 에디터 등)에는 만들지 않는다.
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return (WorldType == EWorldType::Game || WorldType == EWorldType::PIE);
	}

	// 실제 시간으로 매 프레임 호출된다. 화면 효과와 히트스톱이 모두 끝나면 false를 돌려 티커를 해제한다.
	bool TickImpactFrame(float DeltaTime);

	// 화면 효과용 포스트 프로세스 볼륨을 켜거나 끈다. 처음 켤 때 볼륨을 만든다.
	void SetFlashActive(bool bActive, UMaterialInterface* Material = nullptr);

	// 히트스톱을 풀고 원래 게임 속도로 되돌린다.
	void EndHitStop();

	// 티커와 화면 효과, 히트스톱을 모두 정리한다.
	void StopAll();

	FImpactFrameSettings Settings;

	// 연출을 시작한 실제 시간(초)
	double StartRealTime = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> FlashVolume;

	bool bFlashActive = false;

	// 히트스톱 중인지와 시작 전 게임 속도
	bool bHitStopActive = false;
	float SavedTimeDilation = 1.0f;

	FTSTicker::FDelegateHandle TickerHandle;
};
