#pragma once

#include "CoreMinimal.h"
#include "Noise/NoiseTypes.h"

#include "Subsystems/WorldSubsystem.h"
#include "NoiseSubsystem.generated.h"

class ANoiseRingActor;
class UDataTable;

// 위치, 최종 반경, 최종 세기
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnNoiseReported, const FVector&, const float, const float);

/**
 *
 */
UCLASS()
class ECLIPSETHENINJA_API UNoiseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 프로젝트 세팅을 읽어 NoiseTable과 NoiseRingClass를 초기화하는 함수
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 어디에서나 바로 ReportNoise 함수를 사용할 수 있는 함수
	static void	MakeSomeNoise(const UObject* InObject, ENoiseType InType, const FVector& InLocation, AActor* InInstigator, float InScale = 1.f);

	FOnNoiseReported	OnNoiseReported;

protected:
	// 게임/PIE 월드에서만 만든다. 에디터 프리뷰 월드(애니메이션 에디터 등)에는 만들지 않는다.
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return (WorldType == EWorldType::Game || WorldType == EWorldType::PIE);
	}

	// NoiseRingActor를 들어온 값을 이용해 배치하는 함수
	void	ReportNoise(ENoiseType InType, const FVector& InLocation, AActor* InInstigator, float InScale);

private:
	// 소음 위치에 풀에서 링을 꺼내 재생한다. 링은 애니메이션이 끝나면 스스로 풀에 반환된다.
	void	SpawnNoiseRing(const FVector& InLocation, const float InRadius, const float InStrength);
	// 소음의 인스티게이터가 플레이어 혹은 폰인지 확인하는 함수
	static bool IsPlayerNoise(const AActor* InInstigator);

	UPROPERTY(Transient)
	TObjectPtr<UDataTable>			NoiseTable;

	UPROPERTY(Transient)
	TSubclassOf<ANoiseRingActor>	NoiseRingClass;
};
