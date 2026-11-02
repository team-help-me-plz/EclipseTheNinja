

#pragma once

#include "CoreMinimal.h"
#include "Interface/CheckpointResettable.h"

#include "GameFramework/Actor.h"
#include "BreakablePlatform.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UCheckpointBoundComponent;
class UNiagaraComponent;

enum class EBreakablePlatformState
{
	Ready,
	Breaking,
	Broken,
};

UCLASS()
class ECLIPSETHENINJA_API ABreakablePlatform : public AActor, public ICheckpointResettable
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABreakablePlatform();

	virtual void ResetToInitial_Implementation() override;
	virtual void OnAfterReset_Implementation() override {}

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void PostInitializeComponents() override;

	UFUNCTION()
	void OnCollisionBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnCollisionEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void	BreakPlatform();
	void	TryRestore();
	void	RestorePlatform();
	void	StopBreakEffect();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<USceneComponent>	RootScene = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UBoxComponent>			CollisionBox = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UChildActorComponent>	MeshAssembly = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCheckpointBoundComponent>	CheckpointBound = nullptr;

	// 플레이어를 감지한 순간 BreakEffectDuration 동안 재생할 이펙트
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UNiagaraComponent>	BreakEffect = nullptr;

	// BreakEffect 스프라이트 크기 배율 (User.ScaleXY, X = 가로, Y = 세로). 이 이펙트는 컴포넌트 Transform Scale이 반영되지 않으므로 이 값으로 조절한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break|Effect")
	FVector2D	BreakEffectScale = FVector2D(3.f, 3.f);

	// BreakEffect 재생 시간. 이 시간이 지나면 남은 파티클까지 즉시 지운다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break|Effect", meta = (ClampMin = "0.01"))
	float	BreakEffectDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Break|Timer")
	float	BreakTime = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Break|Timer")
	float	RestoreTryTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Break|Timer")
	float	RestoreTime = 1.f;

private:
	FTimerHandle	PlatformBreakTimer;
	FTimerHandle	PlatformRestoreTryTimer;
	FTimerHandle	PlatformRestoreTimer;
	FTimerHandle	BreakEffectTimer;

	EBreakablePlatformState	State = EBreakablePlatformState::Ready;

	int32	OverlapCount = 0;

};
