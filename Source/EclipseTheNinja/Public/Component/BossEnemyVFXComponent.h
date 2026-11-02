#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossEnemyVFXComponent.generated.h"

class ABossEnemy;
class UGameplayAbility;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
struct FAbilityEndedData;
struct FGameplayEffectContextHandle;

/**
 * 보스의 전투 VFX를 한곳에서 관리한다.
 * - 에너지 링크(실드)가 켜져 있는 동안 보스에 실드 이펙트(ShieldSystem)를 붙인다.
 * - 피해를 받으면 피 이펙트(BloodSystem)를 무작위 방향으로, 실드에 막히면 튕기는 이펙트(ShieldHitSystem)를 재생한다.
 * - 보스 패턴(UGABossPatternBase) 어빌리티가 실행되는 동안 칼 본(TrailSocket)에 붙인 리본 트레일을 켠다.
 *   트레일은 칼과 함께 움직이므로 휘두른 궤적과 그대로 일치하고, 칼이 빠르게 움직일 때만 길게 보인다.
 * - 질풍참 어빌리티가 돌진이 끝나는 순간 SpawnGaleGust를 호출한다.
 *
 * 루프로 만들어진 에셋(피)은 잠깐 재생한 뒤 비활성화해, 남은 입자만 마저 재생되고 사라지게 한다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ECLIPSETHENINJA_API UBossEnemyVFXComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossEnemyVFXComponent();

	/**
	 * 질풍참 돌진이 끝난 순간 도착 지점에서 모래 바람이 바깥으로 퍼지게 한다. GustPathSpacing을 켜면 경로 먼지도 남긴다.
	 * 돌진 전 칼집의 반짝임(NS_ChromaticSlash_OmniBurst)은 몽타주 노티파이가 맡는다.
	 */
	void SpawnGaleGust(const FVector& DashStart, const FVector& DashEnd);

	/** 실드 이펙트를 끈다. 보스의 체크포인트 리셋에서 호출한다. */
	void ResetVFX();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// bDrawDebugTrail이 켜져 있으면 칼 본과 트레일의 축을 그린다.
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction
	) override;

	// 보스 패턴 어빌리티가 시작되면 칼 트레일을 켠다.
	void HandleAbilityActivated(UGameplayAbility* Ability);

	// 보스 패턴 어빌리티가 끝나면 칼 트레일을 끈다.
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);

	// 칼 본에 트레일 컴포넌트를 붙여 둔다. 처음 한 번만 만든다.
	void CreateTrail();

	UFUNCTION()
	void HandleEnergyLinkActivated(int32 LinkIndex);

	UFUNCTION()
	void HandleEnergyLinkBroken();

	// 체력이 깎이는 피해를 받았을 때 피를 튀긴다.
	void HandleDamageReceived(const FGameplayEffectContextHandle& Context);

	// 실드(무적)에 피해가 막혔을 때 튕기는 이펙트를 재생한다.
	void HandleDamageBlocked(const FGameplayEffectContextHandle& Context);

	// 공격이 들어온 쪽의 보스 몸통 표면 위치와, 공격자에게서 보스 쪽으로 향하는 수평 방향을 구한다.
	void GetHitPoint(const FGameplayEffectContextHandle& Context, FVector& OutLocation, FVector& OutDirection) const;

	// Delay초 뒤 바닥 위치에 모래 먼지 하나를 피운다.
	void SpawnGustPuffDelayed(const FVector& Location, float Delay);

	// 모래 먼지 하나를 피우고 모래 색(GustLitColor, GustShadowColor)을 입힌다.
	void SpawnGustPuff(const FVector& Location, const FRotator& Rotation);

	// 이펙트를 한 번 재생한다. ActiveTime이 0보다 크면 그 시간 뒤 비활성화해 루프 에셋도 한 번만 나오게 한다.
	UNiagaraComponent* SpawnOneShot(
		const TSoftObjectPtr<UNiagaraSystem>& System,
		const FVector& Location,
		const FRotator& Rotation,
		float Scale,
		float ActiveTime
	) const;

	/** 에너지 링크가 켜져 있는 동안 보스에 붙는 실드 이펙트. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Shield")
	TSoftObjectPtr<UNiagaraSystem> ShieldSystem;

	/** 실드 이펙트 위치. 보스 루트(캡슐 중심) 기준 상대 위치(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Shield")
	FVector ShieldOffset = FVector::ZeroVector;

	/** 실드 이펙트 크기 배율. 보스 액터 스케일이 함께 곱해진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Shield", meta = (ClampMin = "0.01"))
	float ShieldScale = 0.35f;

	/** 피해를 받을 때 튀는 피. 매번 무작위로 돌려 재생한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Hit")
	TSoftObjectPtr<UNiagaraSystem> BloodSystem;

	/** 피 이펙트 크기 배율. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Hit", meta = (ClampMin = "0.01"))
	float BloodScale = 1.0f;

	/** 피 이펙트를 재생하는 시간(초). 에셋이 1초마다 반복되므로 첫 분출만 나오도록 짧게 둔다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Hit", meta = (ClampMin = "0.0", Units = "s"))
	float BloodActiveTime = 0.2f;

	/** 피가 튀는 방향의 무작위 범위. 공격 방향 기준 좌우(Yaw)·위아래(Pitch) 각도(도). 감는 각도(Roll)는 항상 무작위다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Hit", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float BloodYawRange = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Hit", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float BloodPitchRange = 40.0f;

	/** 실드가 켜져 있을 때 공격이 튕겨 나가는 이펙트. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Hit")
	TSoftObjectPtr<UNiagaraSystem> ShieldHitSystem;

	/** 실드 튕김 이펙트 크기 배율. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Hit", meta = (ClampMin = "0.01"))
	float ShieldHitScale = 0.5f;

	/** 피·실드 튕김 이펙트 높이. 보스 캡슐 중심 기준 위쪽 오프셋(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Hit", meta = (Units = "cm"))
	float HitHeightOffset = 30.0f;

	/**
	 * 보스 패턴이 실행되는 동안 칼에 붙는 리본 트레일.
	 * 컴포넌트 위치를 따라 리본을 만들고, 폭(TrailWidthParameter)을 컴포넌트 Z축 방향으로 펼치는 에셋이어야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Trail")
	TSoftObjectPtr<UNiagaraSystem> TrailSystem;

	/** 트레일을 붙일 보스 메시의 본(소켓) 이름. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Trail")
	FName TrailSocket = TEXT("katana_bone");

	/**
	 * 트레일 위치. 칼 본 기준 상대 위치이며 보스 액터 스케일이 곱해진다.
	 * SK_Oni_Samurai의 칼날은 katana_bone의 +Y 방향으로 뻗어 있어, 칼날 가운데에 오도록 Y로 옮긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Trail")
	FVector TrailOffset = FVector(0.0f, 64.0f, 6.0f);

	/** 트레일 회전. 칼 본 기준이며, 트레일의 Z축이 칼날 방향(katana_bone +Y)을 따르게 Roll 90으로 눕힌다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Trail")
	FRotator TrailRotation = FRotator(0.0f, 3.0f, 90.0f);

	/** 리본 폭을 받는 나이아가라 사용자 파라미터. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Trail")
	FName TrailWidthParameter = TEXT("User.TrailSize");

	/** 리본 폭(cm). 칼날 길이에 맞춘다. 보스 액터 스케일은 따로 곱하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Trail", meta = (ClampMin = "0.0", Units = "cm"))
	float TrailWidth = 110.0f;

	/**
	 * 리본 수명 배율. 에셋의 리본 수명(0.07초)에 곱해져 트레일이 그만큼 길게 남는다.
	 * 에셋에 수명 파라미터가 없어 트레일 재생 속도를 1/배율로 늦추는 방식이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Trail", meta = (ClampMin = "0.1"))
	float TrailLifetimeScale = 2.0f;

	/**
	 * 보스 트레일에만 쓸 리본 머티리얼의 원본. 이 머티리얼로 다이내믹 머티리얼을 만들어 색을 바꾼 뒤
	 * 트레일 에셋의 머티리얼 사용자 파라미터(TrailMaterialParameter)에 넣는다. 원본 에셋의 색은 바뀌지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Trail")
	TSoftObjectPtr<UMaterialInterface> TrailMaterial;

	/** 리본 머티리얼을 받는 나이아가라 사용자 파라미터. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Trail")
	FName TrailMaterialParameter = TEXT("User.TrailMaterial");

	/** 트레일 색(머티리얼의 Color 파라미터). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Trail")
	FLinearColor TrailColor = FLinearColor(1.0f, 0.08f, 0.04f, 1.0f);

	/** 트레일 밝기(머티리얼의 Intensity 파라미터). 원본 값은 약 1.09다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Trail", meta = (ClampMin = "0.0"))
	float TrailIntensity = 1.5f;

	/** 칼 본(빨강 X·초록 Y·파랑 Z)과 트레일 위치·Z축(노랑)을 그린다. TrailOffset·TrailRotation을 맞출 때 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Trail")
	bool bDrawDebugTrail = false;

	/** 질풍참 돌풍에 쓰는 모래 먼지. 바닥에서 피어올라 바깥으로 퍼지는 1회분을 여러 개 조합한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Gale Gust")
	TSoftObjectPtr<UNiagaraSystem> GustSystem;

	/** 먼지 하나의 크기 배율. 피어오르는 링 반지름과 퍼지는 속도에 곱해진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Gale Gust", meta = (ClampMin = "0.01"))
	float GustScale = 1.0f;

	/** 모래 밝은 면 색(User.Sand Lit Color). 원본은 짙은 황토색(0.69, 0.38, 0.13)이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Gale Gust")
	FLinearColor GustLitColor = FLinearColor(0.85f, 0.72f, 0.56f, 1.0f);

	/** 모래 그림자 면 색(User.Sand Shadow Color). 원본은 짙은 갈색(0.28, 0.07, 0.004)이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Gale Gust")
	FLinearColor GustShadowColor = FLinearColor(0.52f, 0.42f, 0.33f, 1.0f);

	/** 먼지 하나를 재생하는 시간(초). 에셋이 1초마다 반복되므로 첫 분출만 나오도록 짧게 둔다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Gale Gust", meta = (ClampMin = "0.0", Units = "s"))
	float GustActiveTime = 0.1f;

	/**
	 * 돌진 경로를 따라 먼지를 놓는 간격(cm). 0이면 경로 먼지 없이 도착 지점에서만 퍼진다.
	 * 기본은 0: 돌풍은 돌진이 끝난 자리에서만 일어난다(몽타주의 이전 Gust 이펙트를 대체).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Gale Gust", meta = (ClampMin = "0.0", Units = "cm"))
	float GustPathSpacing = 0.0f;

	/** 경로 먼지가 출발점부터 도착점까지 차례로 피어오르는 데 걸리는 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Gale Gust", meta = (ClampMin = "0.0", Units = "s"))
	float GustPathDuration = 0.12f;

	/** 도착 지점에서 바깥으로 퍼지는 링 수. 첫 링은 발밑 한 점이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Gale Gust", meta = (ClampMin = "0", ClampMax = "6"))
	int32 GustRingCount = 3;

	/** 가장 바깥 링의 반지름(cm). 링마다 같은 간격으로 커진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Gale Gust", meta = (ClampMin = "0.0", Units = "cm"))
	float GustRingRadius = 260.0f;

	/** 바깥 링의 먼지 수. 안쪽 링은 반지름에 비례해 줄어든다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Gale Gust", meta = (ClampMin = "1", ClampMax = "16"))
	int32 GustRingPuffs = 8;

	/** 링이 하나씩 바깥으로 넓어지는 간격(초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Gale Gust", meta = (ClampMin = "0.0", Units = "s"))
	float GustRingInterval = 0.07f;

	/** 링의 깊이(Y) 방향 납작함. 옆에서 보는 화면이라 앞뒤로 덜 퍼지게 해 좌우로 퍼지는 것이 잘 보이게 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss VFX|Gale Gust", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GustRingDepthScale = 0.4f;

private:
	TWeakObjectPtr<ABossEnemy> Boss;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ShieldComp = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> TrailComp = nullptr;

	// 색을 바꾼 보스 전용 트레일 머티리얼
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TrailMID = nullptr;

	// 실행 중인 보스 패턴 어빌리티 수. 0이 되면 트레일을 끈다.
	int32 ActivePatternCount = 0;

	FDelegateHandle AbilityActivatedHandle;
	FDelegateHandle AbilityEndedHandle;
};
