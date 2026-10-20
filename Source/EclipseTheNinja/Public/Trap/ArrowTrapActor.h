

#pragma once

#include "CoreMinimal.h"
#include "Interface/Activatable.h"
#include "Interface/CheckpointResettable.h"

#include "GameFramework/Actor.h"
#include "ArrowTrapActor.generated.h"

class UStaticMeshComponent;
class ATrapArrowActor;
class UCheckpointBoundComponent;

UCLASS()
class ECLIPSETHENINJA_API AArrowTrapActor : public AActor, public IActivatable, public ICheckpointResettable
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AArrowTrapActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void	ToggleActivate_Implementation() override;

	// 체크포인트 리스폰: 발사 쿨다운 초기화 (날아가던 화살은 Subsystem이 런타임 스폰 액터로 풀에 반환)
	virtual void	ResetToInitial_Implementation() override;
	virtual void	OnAfterReset_Implementation() override {}

protected:
	void	SpawnArrow();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent>			RootScene = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UChildActorComponent>	MeshAssembly = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<USceneComponent>			ArrowSpawnPos = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCheckpointBoundComponent>	CheckpointBound = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Trap")
	TSubclassOf<ATrapArrowActor>		ArrowClass = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Trap")
	float								ArrowSpeed = 200.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Trap")
	float								ArrowSpawnDist = 100.f;

	// 오브젝트 풀에 미리 생성을 요청할 화살 수 (동시에 날아가는 화살 수 기준, 화살 풀이 등록된 경우만 적용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|ObjectPool", meta = (ClampMin = "0"))
	int32								ArrowWarmupCount = 5;

private:
	void	ResetActivateCooldown();

	FTimerHandle	ActivateCooldownTimer;
	bool			bIsActivateCooldown = false;
	float			ActivateInterval = 0.5f;

};
