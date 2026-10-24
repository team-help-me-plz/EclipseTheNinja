#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Curves/CurveFloat.h"
#include "Interface/Poolable.h"
#include "HealPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UGameplayEffect;
class APlayerCharacter;

/** 회복 아이템의 연출 단계 */
UENUM(BlueprintType)
enum class EHealPickupState : uint8
{
	/** 풀에서 대기 중 */
	Inactive,

	/** 제자리에서 떠다니는 중 */
	Idle,

	/** 플레이어에게 흡수되는 중 */
	Absorbing,

	/** 수명이 다해 사라지는 중 */
	Expiring,
};

/**
 * 플레이어가 닿으면 흡수되며 GE로 체력을 회복시키는 아이템.
 * 제자리에서 떠다니며 회전하다가 플레이어 캡슐에 닿으면 커브를 따라 크기와 거리를 바꾸며 플레이어에게 빨려 들어간다.
 * 흡수 완료, 수명 만료, 체크포인트 리스폰 시 오브젝트 풀로 반환된다. (풀에 등록되지 않았으면 파괴된다)
 */
UCLASS()
class ECLIPSETHENINJA_API AHealPickup : public AActor, public IPoolable
{
	GENERATED_BODY()

public:
	AHealPickup();

	UFUNCTION(BlueprintPure, Category = "HealPickup")
	EHealPickupState GetPickupState() const { return PickupState; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** 오브젝트 풀: 꺼낸 위치에서 떠다니는 연출과 수명 타이머를 다시 시작한다. */
	virtual void OnSpawnFromPool_Implementation() override;

	/** 오브젝트 풀: 공통 비활성화 전에 연출 상태와 트레일을 정리한다. */
	virtual void OnReturnToPool_Implementation() override;

	UFUNCTION()
	void OnCollisionBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

private:
	/** 떠다니는 상태로 초기화하고 획득 가능 타이머와 수명 타이머를 시작한다. */
	void StartIdle();

	/** 획득 대기 시간이 지나면 충돌을 켜고, 이미 겹쳐 있는 플레이어가 있으면 바로 흡수를 시작한다. */
	void EnablePickup();

	/** 충돌한 컴포넌트가 살아있는 플레이어의 캡슐이면 해당 플레이어를 반환한다. */
	APlayerCharacter* FindAbsorbingPlayer(AActor* OtherActor, UPrimitiveComponent* OtherComp) const;

	/** 흡수 연출을 시작한다. 평상시 이펙트를 끄고 트레일 이펙트를 켠다. */
	void StartAbsorb(APlayerCharacter* Player);

	/** 수명이 다했을 때 작아지며 사라지는 연출을 시작한다. */
	void StartExpire();

	/** 상태별 연출 갱신 */
	void TickIdle(float DeltaSeconds);
	void TickAbsorb(float DeltaSeconds);
	void TickExpire(float DeltaSeconds);

	/** 위아래로 떠다니는 높이와 회전을 VisualRoot에 적용한다. */
	void UpdateFloatingMotion(float DeltaSeconds);

	/** 흡수가 끝났을 때 회복 GE를 적용하고 완료 이펙트를 재생한 뒤 풀로 반환한다. */
	void CompleteAbsorb();

	/** 플레이어에게 회복 GE를 적용한다. */
	void ApplyHeal(APlayerCharacter* Player);

	/** 흡수 목표 위치 (플레이어 캡슐 중심 + 오프셋) */
	FVector GetAbsorbTargetLocation(const APlayerCharacter* Player) const;

	/** 커브 값을 읽는다. 커브가 비어 있으면 DefaultValue를 반환한다. */
	static float EvaluateCurve(const FRuntimeFloatCurve& Curve, float Time, float DefaultValue);

	/** 연출 상태, 타이머, 이펙트를 초기 상태로 되돌린다. */
	void ClearPickupState();

	/** 풀에 반환한다. 풀 액터가 아니면 파괴된다. */
	void ReleasePickup();

protected:
	/** 플레이어 캡슐과의 겹침을 감지하는 루트 충돌체 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> Collision;

	/** 떠다니기/회전/크기 연출이 적용되는 기준 컴포넌트. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> VisualRoot;

	/** 아이템 외형. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** 평상시 이펙트를 재생하는 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> IdleEffectComponent;

	/** 흡수 중 트레일 이펙트를 재생하는 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> AbsorbTrailEffectComponent;

	/** 흡수 완료 시 플레이어에게 적용할 회복 GE. 회복량은 GE의 Health Modifier 수치로 설정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealPickup|Heal")
	TSubclassOf<UGameplayEffect> HealEffectClass;

	/** 스폰 직후 획득할 수 없는 시간(초). 암살 직후 플레이어와 겹쳐 있어도 떠다니는 모습을 잠시 보여준다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealPickup|Lifetime", meta = (ClampMin = "0.0", Units = "s"))
	float PickupEnableDelay = 0.5f;

	/** 스폰 후 자연적으로 사라질 때까지의 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealPickup|Lifetime", meta = (ClampMin = "0.1", Units = "s"))
	float Lifetime = 10.0f;

	/** 수명이 다했을 때 작아지며 사라지는 시간(초). 0이면 즉시 사라진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealPickup|Lifetime", meta = (ClampMin = "0.0", Units = "s"))
	float ExpireShrinkDuration = 0.3f;

	/** 위아래로 떠다니는 높이(cm). 스폰 위치 기준 ±값만큼 움직인다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealPickup|Floating", meta = (ClampMin = "0.0", Units = "cm"))
	float FloatingHeight = 10.0f;

	/** 한 번 오르내리는 데 걸리는 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealPickup|Floating", meta = (ClampMin = "0.05", Units = "s"))
	float FloatingPeriod = 2.0f;

	/** 초당 회전 속도(도) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealPickup|Floating")
	FRotator RotationRate = FRotator(0.0f, 90.0f, 0.0f);

	/** 플레이어에게 흡수되는 데 걸리는 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealPickup|Absorb", meta = (ClampMin = "0.05", Units = "s"))
	float AbsorbDuration = 0.6f;

	/**
	 * 흡수 진행도(0~1)에 따른 거리 비율. 0은 흡수 시작 위치, 1은 플레이어 위치.
	 * 음수면 플레이어 반대 방향으로 밀려난다. 끝 값은 1이어야 플레이어에게 도달한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealPickup|Absorb")
	FRuntimeFloatCurve AbsorbDistanceCurve;

	/**
	 * 흡수 진행도(0~1)에 따른 크기 배율. 1은 원래 크기.
	 * 끝 값은 캐릭터에 가려질 만큼 작아야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealPickup|Absorb")
	FRuntimeFloatCurve AbsorbScaleCurve;

	/** 흡수 목표 위치. 플레이어 캡슐 중심 기준 오프셋 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealPickup|Absorb")
	FVector AbsorbTargetOffset = FVector::ZeroVector;

	/** 평상시 이펙트 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealPickup|Effects")
	TObjectPtr<UNiagaraSystem> IdleEffect;

	/** 흡수 중 이펙트 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealPickup|Effects")
	TObjectPtr<UNiagaraSystem> AbsorbEffect;

	/** 흡수 완료 후 체력이 회복될 때 플레이어에게 붙여 재생할 이펙트 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealPickup|Effects")
	TObjectPtr<UNiagaraSystem> HealCompleteEffect;

	/** 회복 완료 이펙트 위치. 플레이어 캡슐 중심 기준 오프셋 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealPickup|Effects")
	FVector HealCompleteEffectOffset = FVector::ZeroVector;

private:
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "HealPickup")
	EHealPickupState PickupState = EHealPickupState::Inactive;

	/** 흡수 중인 플레이어 */
	TWeakObjectPtr<APlayerCharacter> AbsorbingPlayer;

	/** 현재 상태가 시작된 뒤 지난 시간 */
	float StateElapsed = 0.0f;

	/** 떠다니기 연출 누적 시간 */
	float FloatingElapsed = 0.0f;

	/** 흡수 시작 시점의 위치와 위아래 흔들림 값 */
	FVector AbsorbStartLocation = FVector::ZeroVector;
	float AbsorbStartFloatingOffset = 0.0f;

	FTimerHandle PickupEnableTimer;
	FTimerHandle LifetimeTimer;

};
