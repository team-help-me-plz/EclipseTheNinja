#pragma once

#include "CoreMinimal.h"
#include "Interface/Poolable.h"

#include "GameFramework/Actor.h"
#include "NoiseRingActor.generated.h"

class UWidgetComponent;
class UNoiseRingWidget;
class USphereComponent;

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

protected:
	void	NotifyEnemies(const FVector& InLocation, const float InRadius, const float InStrength);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Noise|Ring")
	TObjectPtr<UWidgetComponent>	NoiseWidgetComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Noise|Ring")
	TObjectPtr<USphereComponent>	NoiseSphereComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Noise|Ring")
	float	WidgetDrawSize = 256.f;

private:
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
