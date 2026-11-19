#pragma once

#include "CoreMinimal.h"
#include "Interface/Poolable.h"

#include "GameFramework/Actor.h"
#include "NoiseRingActor.generated.h"

class UWidgetComponent;
class UNoiseRingWidget;
class USphereComponent;
class UNiagaraSystem;

UCLASS()
class ECLIPSETHENINJA_API ANoiseRingActor : public AActor, public IPoolable
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ANoiseRingActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void OnSpawnFromPool_Implementation() override;
	virtual void OnReturnToPool_Implementation() override;

	// 소음 위치에 반경 크기로 링을 띄운다. 애니메이션이 끝나면 풀로 반환된다.
	void	Play(const FVector& InLocation, const float InRadius, const float InStrength);

	// 월드에 떠 있는 소음 링과 파동 이펙트를 모두 즉시 치운다 (컷신이 시작될 때 화면에 남지 않게)
	static void	StopAllActive(const UObject* WorldContextObject);

protected:
	void	NotifyEnemies(const FVector& InLocation, const float InRadius, const float InStrength);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Noise|Ring")
	TObjectPtr<UWidgetComponent>	NoiseWidgetComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Noise|Ring")
	TObjectPtr<USphereComponent>	NoiseSphereComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Noise|Ring")
	float	WidgetDrawSize = 256.f;

	// false면 UI 링 위젯을 띄우지 않고, 적 알림·파동 이펙트만 처리한 뒤 바로 풀로 반환한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Noise|Ring")
	bool	bShowRingWidget = true;

	// 소음 위치에서 한 번 퍼지는 파동 이펙트. 비어 있으면 띄우지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Noise|Wave")
	TObjectPtr<UNiagaraSystem>	NoiseWaveSystem;

	// 파동 이펙트에 소음 반경(cm)을 넘길 User 파라미터 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Noise|Wave")
	FName	NoiseRadiusParamName = TEXT("User.NoiseRadius");

private:
	// 소음 위치에 파동 이펙트를 띄우고 반경을 넘긴다.
	void	SpawnNoiseWave(const FVector& InLocation, const float InRadius);

	// 위젯 인스턴스를 만들고 종료 이벤트를 한 번만 바인딩한다.
	bool	EnsureWidget();

	// 월드 반경을 InPC 화면 기준 크기(위젯 단위)로 투영해 지름을 구한다. 투영할 수 없으면 false.
	static bool	CalcScreenDiameter(APlayerController* InPC, const FVector& InLocation, const float InRadius, float& OutDiameter);

	void	OnNoiseEnd();
	void	ReleaseToPool();

	TWeakObjectPtr<UNoiseRingWidget>	NoiseRingWidget = nullptr;

	// 풀 반환 중 애니메이션을 멈출 때 OnAnimationFinished가 다시 들어와도 무시하기 위한 상태
	bool			bIsPlaying = false;

};
