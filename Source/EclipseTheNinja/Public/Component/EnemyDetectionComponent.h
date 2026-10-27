#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/Enemy.h"
#include "Engine/TimerHandle.h"
#include "EnemyDetectionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEnemyAwarenessChanged, float, Gauge, EEnemyAlertLevel, Level);
class UEnemyAwarenessWidget;

/** 방향과 무관한 두 감지 원과 장애물 검사를 통해 경계 상태를 판단한다. */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ECLIPSETHENINJA_API UEnemyDetectionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UEnemyDetectionComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** UI용 0~1 게이지와 경계 수준. 값이 변경될 때만 알린다. */
    UPROPERTY(BlueprintAssignable, Category="AI|Detection|UI")
    FEnemyAwarenessChanged OnAwarenessChanged;
    
    /** 실제 감지에 사용하는 값*/
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="AI|Detection")
    float AlertGauge = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="AI|Detection")
    EEnemyAlertLevel AlertLevel = EEnemyAlertLevel::Normal;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="AI|Detection")
    bool bHasLineOfSight = false;

    /** 판정 주기와 분리해 매 프레임 부드럽게 갱신하는 표시용 값. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="AI|Detection|UI")
    float DisplayedGauge = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|UI")
    bool bShowDiscoveryRange = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|UI")
    bool bShowAwarenessGauge = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|UI", meta=(Units="cm"))
    float GaugeHeight = 160.0f;

    /** 색상과 표시 스타일을 편집할 WBP. 기본값은 Widgets/WBP_EnemyAwareness. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Detection|UI")
    TSubclassOf<UEnemyAwarenessWidget> AwarenessWidgetClass;

    /** 감지범위 위젯을 보이게 할지 결정하는 값. True를 기본 상태로 두고 숨어있어야만하는 적일 경우 false로 만든다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|UI")
    bool bShowAwarenessWidget = true;

    /** 디버그에서만 큰 감지 원을 추가로 표시한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|Debug")
    bool bDrawDebugSight = false;

    UFUNCTION(BlueprintPure, Category="AI|Detection")
    float GetCurrentDetectionRadius() const;

    UFUNCTION(BlueprintPure, Category="AI|Detection")
    float GetCurrentDiscoveryRadius() const;

    UFUNCTION(BlueprintPure, Category = "AI|Detection")
    EEnemyAlertLevel GetCurrentAlertLevel() const { return AlertLevel; }

    /** 공격을 받으면 감지 상태와 무관하게 즉시 전투 상태가 되고, 공격자 위치를 마지막 목격 위치로 기록한다. */
    UFUNCTION(BlueprintCallable, Category="AI|Detection")
    void NotifyDamaged(AActor* DamageInstigator);

    /** 체크포인트 리스폰용. 경계 게이지/단계와 마지막 목격 정보를 초기화하고, 사망 시 꺼진 틱과 UI를 되살린다. */
    void ResetDetection();

    /** 오브젝트 풀 반환용. 감지 틱과 위젯 재시도 타이머를 멈추고 발각 범위 UI를 제거한다. (ResetDetection으로 다시 시작) */
    void StopDetection();

    /** 현재 감지 상태를 Blackboard에 다시 기록한다. (BT 재시작 시 사용) */
    void RefreshBlackboard() { UpdateBlackboard(); }

	void NotifyNoise(const FVector& InLocation, const float InRadius, const float InStrength);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|Sight", meta=(ClampMin="1.0", Units="cm"))
    float DetectionRadius = 600.0f;

    /** 이 안에서 적이 플레이어 쪽을 바라보고 있고 보이면 게이지와 무관하게 즉시 전투 상태가 된다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|Sight", meta=(ClampMin="1.0", Units="cm"))
    float DiscoveryRadius = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|Sight", meta=(ClampMin="1.0"))
    float AlertRangeMultiplier = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|Sight", meta=(Units="cm"))
    float EyeHeight = 50.0f;

    /** 장애물은 이 채널을 Block해야 시야를 차단한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|Sight")
    TEnumAsByte<ECollisionChannel> SightTraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|Awareness", meta=(ClampMin="0.01", Units="s"))
    float GaugeFillTime = 2.0f;

    /** 적이 플레이어를 등지고 있을 때 게이지 충전 속도 배율. 0.5면 등 뒤에서는 GaugeFillTime의 두 배가 걸린다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|Awareness", meta=(ClampMin="0.0", ClampMax="1.0"))
    float BehindGaugeFillRate = 0.5f;

    /** 일반 상태에서 감지를 놓쳤을 때 미완성 게이지가 비워지는 시간. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|Awareness", meta=(ClampMin="0.01", Units="s"))
    float GaugeDecayTime = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|Awareness", meta=(ClampMin="0.0", Units="s"))
    float ForgetDelay = 10.0f;

    /** 레이캐스트 검사 간격. 화면 표시는 이 값과 무관하게 매 프레임 갱신한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|Sight", meta=(ClampMin="0.02", Units="s"))
    float DetectionInterval = 0.1f;

    /** Noise 감지 시 주목도가 내려가지 않는 시간 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Detection|Noise", meta=(ClampMin="0.02", Units="s"))
    float NoiseAlertMaintain = 1.0f;

private:
	// 소음 발생 후 일정시간동안 주목도 하한선을 유지하게 하는 함수
	void	NoiseAlertStart(float InTargetGauge);
	// 소음 발생 후 일정시간 뒤 주목도 하한선을 해제하는 함수
	void	NoiseAlertReset();
    // 플레이어와 분신을 같은 기준으로 검사하고 가장 가까운 가시 대상을 선택한다.
    APawn* FindVisibleTarget(float Radius) const;
    // 소유한 액터가 파라미터로 들어온 타겟을 볼 수 있는지 반환하는 함수
    bool CanSeeTarget(const AActor* Target, float Radius) const;
    // 감지 로직 업데이트 함수
    void UpdateDetection(float DeltaTime);
    // BB용 변수 업데이트 함수
    void UpdateBlackboard();
    // 발각 범위 위젯 생성. 플레이어가 준비되지 않았을 때만 재시도한다.
    void CreatePresentationWidget();

    FTimerHandle PresentationRetryTimer;

    UPROPERTY(Transient)
    TObjectPtr<UEnemyAwarenessWidget> PresentationWidget;

    UPROPERTY(Transient)
    TWeakObjectPtr<AEnemy> Owner;

    UPROPERTY(Transient)
    TWeakObjectPtr<APawn> VisibleTarget;

    FVector LastKnownLocation = FVector::ZeroVector;
    bool bHasLastKnownLocation = false;
    float TimeWithoutSight = 0.0f;
    float DetectionElapsed = 0.0f;

	FTimerHandle	NoiseTimer;
	bool			bIsNoise = false;
	float			NoiseAlertMin = 0.0f;
	FVector			NoiseLocation = FVector::ZeroVector;
};
