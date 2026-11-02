

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/TimerHandle.h"
#include "Engine/StreamableManager.h"
#include "GAS/Ability/GACooldownBase.h"
#include "GASwiftStrike.generated.h"

class ACharacter;
class UCurveFloat;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FCollisionShape;
struct FCollisionQueryParams;
struct FCollisionResponseParams;

/**
 *
 */
UCLASS()
class ECLIPSETHENINJA_API UGASwiftStrike : public UGACooldownBase
{
	GENERATED_BODY()

public:
	UGASwiftStrike();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;
	// 어빌리티를 받을 때 몽타주·VFX를 미리 불러 두고 잔상 텍스처를 만든다. 첫 시전 때 로딩으로 멈추지 않게 한다.
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;


	static FGameplayTag GetSwiftStrikeAbilityTag();
	// 질풍참 시전 중 상태 태그 (이동·커서 회전 차단용)
	static FGameplayTag GetSwiftStrikingTag();

protected:
	UFUNCTION()
	void OnSwiftStrikeFinished();

	UFUNCTION()
	void OnSwiftStrikeInterrupted();

	UFUNCTION()
	void OnSwiftStrikeMontageCompleted();

private:
	void RestoreDashState();
	// 끝점이 막히는 적과 겹치면 적 뒤 빈 곳으로 옮기고, 없으면 원래 끝점을 쓴다.
	FVector ResolveDashEnd(const FVector& Start, const FVector& End, const FVector& Dir, const FCollisionShape& Shape,
		ECollisionChannel MoveChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& WorldResponse, ECollisionResponse PawnResponse) const;
	void StopDashRootMotion();
	UCurveFloat* GetDashTimeCurve();
	void StartHitStop();
	void EndHitStop();
	void CollectTargets(const ACharacter* Player, const FVector& Start, const FVector& End, const FCollisionShape& Shape);
	void ApplyHitToTargets();
	void StartDashEffects(ACharacter* Player, float DashDuration);
	void UpdateDashSlash();
	void StopDashEffects();
	void OnAssetsPreloaded();
	// 잔상이 겹쳐 보이는 최대 개수. 수명과 간격으로 정한다.
	int32 GetAfterimageSlotCount() const;
	// 모자란 만큼만 잔상 텍스처와 머티리얼을 만든다. 이미 있는 것은 재사용한다.
	void EnsureAfterimagePool(UMaterialInterface* Material);
	// 캡처 컴포넌트는 한 번 만들어 두고 돌진마다 재사용한다.
	void EnsureAfterimageCapture(ACharacter* Player);
	void StartAfterimages(ACharacter* Player);
	void EmitAfterimage();
	void PlayImpactEffect(const FVector& Location) const;
	void DrawDebugHitbox(const FVector& Start, const FVector& End, const FCollisionShape& Shape) const;

protected:
	// 질풍참의 돌진 거리
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike", meta = (ClampMin = "0.0", Units = "cm"))
	float DashDistance = 500.f;

	// 질풍참 시전시간
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike", meta = (ClampMin = "0.0", Units = "s"))
	float Duration = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike", meta = (ClampMin = "0.0"))
	float Damage = 30.f;

	// 돌진 진행 곡선(가로: 정규화 시간, 세로: 정규화 이동 거리). 비어 있으면 DashEaseExponent로 감속 곡선을 만든다.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Feel")
	TObjectPtr<UCurveFloat> DashTimeCurve;

	// 클수록 출발 직후 대부분을 이동하고 끝에서 급하게 멈춘다. 1이면 등속.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Feel", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float DashEaseExponent = 3.f;

	// 돌진이 끝난 뒤 남기는 속도. 0이면 그 자리에서 멈추고, 값이 있으면 미끄러지며 선다.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Feel", meta = (ClampMin = "0.0", Units = "cm/s"))
	float DashEndSpeed = 300.f;

	// 끝점이 적과 겹칠 때 적을 관통해 더 나아갈 수 있는 최대 거리
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Collision", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxPassThroughDistance = 150.f;

	// 겹치지 않는 끝점을 찾을 때 한 번에 옮기는 거리
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Collision", meta = (ClampMin = "1.0", Units = "cm"))
	float EndSearchStep = 10.f;

	// 적을 맞혔을 때 플레이어와 맞은 적을 멈추는 시간(실제 시간). 0이면 끈다.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Feel", meta = (ClampMin = "0.0", Units = "s"))
	float HitStopDuration = 0.07f;

	// 히트스톱 동안의 시간 배속
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Feel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HitStopTimeDilation = 0.05f;

	// 질풍참 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike")
	TSoftObjectPtr<UAnimMontage> SwiftStrikeMontage;

	// 공중에서 시전하면 지상 베기 대신 전용 전신 돌진 포즈를 사용한다.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike")
	TSoftObjectPtr<UAnimMontage> AirSwiftStrikeMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Effect")
	TSubclassOf<UGameplayEffect> DamageEffect;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX", meta = (DisplayName = "Dash Slash Effect", ToolTip = "가로 가르기에 사용할 Niagara 에셋을 지정합니다."))
	TSoftObjectPtr<UNiagaraSystem> DashSlashEffect;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX", meta = (ToolTip = "캐릭터 잔상에 사용할 Niagara 에셋을 지정합니다."))
	TSoftObjectPtr<UNiagaraSystem> AfterimageEffect;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX", meta = (ToolTip = "캐릭터 자세 캡처를 표시할 잔상 머티리얼을 지정합니다."))
	TSoftObjectPtr<UMaterialInterface> AfterimageMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX", meta = (ToolTip = "명중 시 재생할 Niagara 에셋을 지정합니다."))
	TSoftObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX", meta = (Units = "cm", ToolTip = "기존 가로 가르기 전체를 돌진 방향 앞쪽으로 옮기는 거리입니다."))
	float DashSlashForwardOffset = 60.f;

	// 가로 베기는 캐릭터 중심을 기준으로 시전 방향에 맞춰 배치한다.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX")
	FVector DashSlashOffset = FVector(0.f, 0.f, 10.f);

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX")
	FRotator DashWindRotation = FRotator::ZeroRotator;

	// 가로 길이는 실제 이동 거리로 고정하고, 스케일은 선의 두께에만 적용한다.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX", meta = (ClampMin = "0.01", DisplayName = "Dash Slash Width Scale"))
	float DashWindScale = 0.32f;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX", meta = (ClampMin = "0.03", Units = "s"))
	float AfterimageInterval = 0.045f;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX", meta = (ClampMin = "0.01", ClampMax = "0.3", Units = "s"))
	float AfterimageLifetime = 0.18f;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX", meta = (ClampMin = "150.0", Units = "cm"))
	float AfterimageSize = 300.f;

	// 잔상 캡처 텍스처의 가로·세로 픽셀 수. 클수록 선명하지만 캡처 비용과 메모리가 늘어난다.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX", meta = (ClampMin = "64", ClampMax = "1024"))
	int32 AfterimageResolution = 256;

	// 잔상의 기본 투명도. AfterimageColor의 A와 곱해진다.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AfterimageOpacity = 0.4f;

	// RGB는 색과 발광 세기, A는 각 효과의 기존 감쇠에 곱할 투명도다.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX|Color", meta = (DisplayName = "Dash Slash Color", ToolTip = "돌진 중 가로 베기 섬광의 색상. RGB는 HDR 발광 세기, A는 투명도 배율입니다."))
	FLinearColor DashWindColor = FLinearColor(0.55f, 0.85f, 1.1f, 0.65f);

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX|Color", meta = (ToolTip = "캐릭터 자세 잔상의 색상. A는 기존 잔상 투명도와 수명 감쇠에 곱해집니다."))
	FLinearColor AfterimageColor = FLinearColor(0.15f, 0.7f, 1.5f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX|Color", meta = (ToolTip = "적에게 명중할 때 발생하는 효과의 기본 색상입니다."))
	FLinearColor ImpactColor = FLinearColor(0.8f, 2.f, 3.f, 1.f);

	// 색상환 한 바퀴가 1이다. 아래 네 값을 모두 0으로 두면 기본 색상의 색조를 유지한다.
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX|Color", AdvancedDisplay, meta = (ToolTip = "명중 효과가 생성될 때 더하는 색조 이동량입니다. 색상환 한 바퀴가 1입니다."))
	float ImpactHueShiftStart = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX|Color", AdvancedDisplay, meta = (ToolTip = "명중 효과가 사라질 때의 색조 이동량입니다. 0이면 선택한 색조를 유지합니다."))
	float ImpactHueShiftEnd = -0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX|Color", AdvancedDisplay, meta = (ToolTip = "명중 파티클마다 무작위로 더할 색조 이동량의 최솟값입니다."))
	float ImpactHueRandomMin = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX|Color", AdvancedDisplay, meta = (ToolTip = "명중 파티클마다 무작위로 더할 색조 이동량의 최댓값입니다."))
	float ImpactHueRandomMax = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX", meta = (ClampMin = "0.01"))
	float ImpactScale = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|VFX", meta = (ClampMin = "0.01", Units = "s"))
	float ImpactLifetime = 0.3f;
	// 돌진 판정 캡슐의 시작·끝과 쓸고 지나간 경로, 맞은 적을 그린다
	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Debug")
	bool bDrawDebugHitbox = false;

	UPROPERTY(EditDefaultsOnly, Category = "SwiftStrike|Debug", meta = (EditCondition = "bDrawDebugHitbox", ClampMin = "0.0", Units = "s"))
	float DebugDrawTime = 2.f;


private:
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveDashSlash;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedImpactEffect;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedAfterimageEffect;

	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponent2D> AfterimageCapture;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextureRenderTarget2D>> AfterimageTextures;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> AfterimageMaterials;

	TArray<double> AfterimageReuseTimes;
	FTimerHandle AfterimageTimerHandle;

	// 미리 불러 둔 에셋이 메모리에서 내려가지 않게 잡아 둔다. 어빌리티를 잃으면 놓는다.
	TSharedPtr<FStreamableHandle> PreloadHandle;
	FTimerHandle DashSlashTimerHandle;
	FVector DashSlashStart = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> LoadedSwiftStrikeMontage;

	TArray<FHitResult> AttackTargets;

	// DashTimeCurve가 비어 있을 때 쓰는 감속 곡선
	UPROPERTY(Transient)
	TObjectPtr<UCurveFloat> GeneratedDashTimeCurve;
	float GeneratedDashEaseExponent = 0.f;

	uint16 DashRootMotionSourceID = 0;
	FVector DashDirection = FVector::ZeroVector;
	FTimerHandle DashTimerHandle;

	FTimerHandle HitStopTimerHandle;
	TArray<TWeakObjectPtr<AActor>> HitStopActors;

	bool bDashFinished = false;
	bool bMontageFinished = true;
	bool bUsingAirMontage = false;
	bool bDashInvincibilityAdded = false;
	bool bDashCollisionChanged = false;
	ECollisionResponse SavedPawnCollisionResponse = ECR_Block;


};
