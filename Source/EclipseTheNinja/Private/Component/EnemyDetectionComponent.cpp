#include "Component/EnemyDetectionComponent.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "CollisionQueryParams.h"
#include "Widgets/EnemyAwarenessWidget.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

namespace EnemyDetectionKeys
{
    const FName TargetActor(TEXT("TargetActor"));
    const FName HasSight(TEXT("bHasLineofSight"));
    const FName LastLocation(TEXT("LastKnownLocation"));
    const FName HasLastLocation(TEXT("bHasLastKnownLocation"));
    const FName InAttackRange(TEXT("bInAttackRange"));
    const FName IsDead(TEXT("bIsDead"));
    const FName IsHitReacting(TEXT("bIsHitReacting"));
    const FName AlertLevel(TEXT("AlertLevel"));
}

UEnemyDetectionComponent::UEnemyDetectionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    static ConstructorHelpers::FClassFinder<UEnemyAwarenessWidget> WidgetBlueprint(TEXT("/Game/Widgets/WBP_EnemyAwareness"));
    AwarenessWidgetClass = WidgetBlueprint.Class;
    if (!AwarenessWidgetClass) AwarenessWidgetClass = UEnemyAwarenessWidget::StaticClass();

}

void UEnemyDetectionComponent::NotifyNoise(const FVector& InLocation, const float InRadius, const float InStrength)
{
	if (!Owner.Get() || !Owner.Get()->IsReadyForAI() || AlertLevel == EEnemyAlertLevel::Combat)
	{
		return ;
	}
	// 소음 감지 구와 같은 기준(경계 시 확대된 반경)을 쓴다.
	const float DetectionR = GetCurrentDetectionRadius();
	const float DiscoveryR = GetCurrentDiscoveryRadius();
	// 소음 원에서 적에게 가장 가까운 지점까지의 거리
	const float	Dist = FMath::Max(0.0f, FVector::Dist(Owner.Get()->GetActorLocation(), InLocation) - InRadius);

	// 빨간 원 안이면 세기 최대(바로 100), 바깥은 노란 경계(0) ~ 빨간 경계(1) 사이 선형 감쇠
	const float Strength = Dist <= DiscoveryR ? 1.0f : InStrength;
	const float Ratio = FMath::Clamp((DetectionR - Dist) / FMath::Max(1.0f, DetectionR - DiscoveryR), 0.f, 1.f);
	const float Gain = Strength * Ratio;

	if (Gain < 0.001f)
	{
		return ;
	}
	// 반영된 소리만 수색 후보 위치로 기록한다. 게이지가 가득 차면 UpdateDetection이 이 위치를 수색 지점으로 넘긴다.
	this->NoiseLocation = InLocation;
	this->NoiseAlertStart(FMath::Min(1.0f, AlertGauge + Gain));
}

void UEnemyDetectionComponent::BeginPlay()
{
    Super::BeginPlay();

    Owner = Cast<AEnemy>(GetOwner());
    if (!Owner.IsValid())
    {
        SetComponentTickEnabled(false);
        return;
    }

    UpdateBlackboard();
    CreatePresentationWidget();
}

float UEnemyDetectionComponent::GetCurrentDetectionRadius() const
{
    const float Scale = AlertLevel == EEnemyAlertLevel::Normal ? 1.0f : FMath::Max(1.0f, AlertRangeMultiplier);
    return FMath::Max(1.0f, DetectionRadius) * Scale;
}

float UEnemyDetectionComponent::GetCurrentDiscoveryRadius() const
{
    const float Scale = AlertLevel == EEnemyAlertLevel::Normal ? 1.0f : FMath::Max(1.0f, AlertRangeMultiplier);
    // 잘못 설정하더라도 작은 발각 범위가 감지 범위를 넘지 않게 한다.
    return FMath::Clamp(DiscoveryRadius, 1.0f, FMath::Max(1.0f, DetectionRadius)) * Scale;
}

void UEnemyDetectionComponent::NotifyDamaged(AActor* DamageInstigator)
{
    AEnemy* OwnerEnemy = Owner.Get();
    if (!OwnerEnemy || !OwnerEnemy->IsReadyForAI()) return;

    // 투사체의 실제 공격자도 확인한다. 알 수 없다면 본체를 무조건 지목하지 않고
    // 현재 보이는 플레이어/분신 중 같은 기준으로 대상을 고른다.
    APawn* Attacker = Cast<APawn>(DamageInstigator);
    if (!OwnerEnemy->IsValidAITarget(Attacker))
        Attacker = IsValid(DamageInstigator) ? DamageInstigator->GetInstigator() : nullptr;
    if (!OwnerEnemy->IsValidAITarget(Attacker))
        Attacker = FindVisibleTarget(GetCurrentDetectionRadius());
    if (!OwnerEnemy->IsValidAITarget(Attacker)) return;

    const EEnemyAlertLevel PreviousLevel = AlertLevel;
    const float PreviousGauge = AlertGauge;

    // 등 뒤에서 맞아도 바로 전투 상태가 된다. 공격자 위치를 마지막 목격 위치로 기록한다.
    AlertLevel = EEnemyAlertLevel::Combat;
    AlertGauge = 1.0f;
    TimeWithoutSight = 0.0f;
    LastKnownLocation = Attacker->GetActorLocation();
    bHasLastKnownLocation = true;

    if (PreviousLevel != AlertLevel || !FMath::IsNearlyEqual(PreviousGauge, AlertGauge))
        OnAwarenessChanged.Broadcast(AlertGauge, AlertLevel);

    UE_LOG(LogTemp, Log, TEXT("[EnemyDetection] %s damaged by %s: %s -> Combat"), *OwnerEnemy->GetName(),
        *Attacker->GetName(), *UEnum::GetValueAsString(PreviousLevel));

    // 다음 판정 주기를 기다리지 않고 시야와 Blackboard를 바로 갱신한다.
    DetectionElapsed = 0.0f;
    UpdateDetection(0.0f);
}

void UEnemyDetectionComponent::ResetDetection()
{
    AlertLevel = EEnemyAlertLevel::Normal;
    AlertGauge = 0.0f;
    DisplayedGauge = 0.0f;
    bHasLineOfSight = false;
    VisibleTarget.Reset();
    LastKnownLocation = FVector::ZeroVector;
    bHasLastKnownLocation = false;
    TimeWithoutSight = 0.0f;
    DetectionElapsed = 0.0f;

    if (!Owner.IsValid()) return;

    // 사망 시 꺼진 틱과 발각 범위 UI를 복원한다.
    SetComponentTickEnabled(true);
    if (PresentationWidget)
    {
        if (!PresentationWidget->IsInViewport())
            PresentationWidget->AddToViewport(5);
    }
    else
    {
        CreatePresentationWidget();
    }

    UpdateBlackboard();
    OnAwarenessChanged.Broadcast(AlertGauge, AlertLevel);
}

void UEnemyDetectionComponent::StopDetection()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PresentationRetryTimer);
    }

    if (PresentationWidget)
        PresentationWidget->RemoveFromParent();

    SetComponentTickEnabled(false);
	NoiseAlertReset();
}

void UEnemyDetectionComponent::NoiseAlertStart(float InTargetGauge)
{
	this->bIsNoise = true;
	// 하한만 갱신한다. 게이지와 단계는 UpdateDetection이 이 값을 보고 바꾼다.
	// 유지 중에 다시 들으면 더 높은 쪽을 하한으로 삼는다.
	this->NoiseAlertMin = FMath::Max(this->NoiseAlertMin, FMath::Min(1.0f, InTargetGauge));
	UWorld* World = GetWorld();
	if (!World)
	{
		return ;
	}
	World->GetTimerManager().SetTimer(
		this->NoiseTimer,
		this,
		&UEnemyDetectionComponent::NoiseAlertReset,
		this->NoiseAlertMaintain,
		false
	);
}

void UEnemyDetectionComponent::NoiseAlertReset()
{
	this->NoiseAlertMin = 0.0f;
	this->bIsNoise = false;
	this->NoiseLocation = FVector::ZeroVector;
	// 풀 생성/반환이나 월드 정리 중에도 불리므로 월드가 없을 수 있다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(this->NoiseTimer);
	}
}

APawn* UEnemyDetectionComponent::FindVisibleTarget(float Radius) const
{
    const AEnemy* OwnerEnemy = Owner.Get();
    if (!OwnerEnemy || !GetWorld()) return nullptr;

    APawn* BestTarget = nullptr;
    double BestDistanceSquared = TNumericLimits<double>::Max();
    bool bBestDiscovered = false;
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        APawn* Candidate = *It;
        if (!OwnerEnemy->IsValidAITarget(Candidate) || !CanSeeTarget(Candidate, Radius)) continue;

        const FVector Offset = Candidate->GetActorLocation() - OwnerEnemy->GetActorLocation();
        const double DistanceSquared = Offset.SizeSquared();
        const bool bFacing = FMath::Abs(Offset.X) <= 10.0f
            || OwnerEnemy->GetActorForwardVector().X * Offset.X > 0.0f;
        const bool bDiscovered = bFacing && DistanceSquared <= FMath::Square(GetCurrentDiscoveryRadius());
        // 등 뒤의 가까운 대상이 정면 발각 영역의 다른 대상을 가리지 않게 한다.
        // 같은 발각 조건이면 종류와 무관하게 가까운 대상을 선택한다.
        if (!BestTarget || (bDiscovered && !bBestDiscovered)
            || (bDiscovered == bBestDiscovered && DistanceSquared < BestDistanceSquared))
        {
            BestTarget = Candidate;
            BestDistanceSquared = DistanceSquared;
            bBestDiscovered = bDiscovered;
        }
    }
    return BestTarget;
}

bool UEnemyDetectionComponent::CanSeeTarget(const AActor* Target, float Radius) const
{
    const AEnemy* OwnerEnemy = Owner.Get();
    if (!OwnerEnemy || !IsValid(Target) || !GetWorld()) return false;

    // 표시 원과 같은 캐릭터 중심/반경을 사용한다. 바라보는 방향은 검사하지 않는다.
    if (FVector::DistSquared(Target->GetActorLocation(), OwnerEnemy->GetActorLocation()) > FMath::Square(Radius)) return false;

    // 이 컴포넌트를 소유한 액터의 눈 위치
    const FVector Eye = OwnerEnemy->GetActorLocation() + FVector::UpVector * EyeHeight;

    // 타겟의 머리(눈), 몸통(중심), 발 근처 중 한 곳이라도 보이면 감지한다.
    FVector TargetEye;
    FRotator TargetEyeRotation;
    Target->GetActorEyesViewPoint(TargetEye, TargetEyeRotation);
    const FVector TargetCenter = Target->GetActorLocation();
    const FVector TargetFeet = TargetCenter - FVector::UpVector * Target->GetSimpleCollisionHalfHeight() * 0.8f;
    const FVector SightPoints[] = { TargetEye, TargetCenter, TargetFeet };

    FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemySight), false, OwnerEnemy);
    for (const FVector& Point : SightPoints)
    {
        // 소유 액터의 눈 위치에서 타겟의 각 부위로 라인캐스트
        FHitResult Hit;
        const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point, SightTraceChannel, Params);
        // visibility에 감지되지 않거나, 감지된 액터가 타겟이면 보이는 것으로 판단
        if (!bBlocked || Hit.GetActor() == Target) return true;
    }
    return false;
}

void UEnemyDetectionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AEnemy* OwnerEnemy = Owner.Get();
    if (!OwnerEnemy) return;

    // 소유 액터가 죽은 상태이면 
    if (OwnerEnemy->IsDead())
    {
        UpdateDetection(0.0f);
        GetWorld()->GetTimerManager().ClearTimer(PresentationRetryTimer);
        
        // 발각 범위를 나타내는 UI를 제거
        if (PresentationWidget) 
            PresentationWidget->RemoveFromParent();

        // 틱 비활성화
        SetComponentTickEnabled(false);
		NoiseAlertReset();
        return;
    }

    //시간 누적
    DetectionElapsed += DeltaTime;

    // 최대 0.02f초에 한 번 감지상태 갱신
    if (DetectionElapsed >= FMath::Max(0.02f, DetectionInterval))
    {

        const float Elapsed = DetectionElapsed;
        DetectionElapsed = 0.0f;
        UpdateDetection(Elapsed);
    }

    // 경계 단계가 Normal이면 
    // 표시 값을 실제 값으로 보간 (게이지가 차오르는 형태) 
    // 경계/공격 상태에는 가득찬 상태로 표시
    DisplayedGauge = AlertLevel == EEnemyAlertLevel::Normal
        ? FMath::FInterpTo(DisplayedGauge, AlertGauge, DeltaTime, 12.0f) : 1.0f;

    // 표시 값이 작으면 0으로 밀기
    if (AlertGauge == 0.0f && DisplayedGauge < 0.001f) DisplayedGauge = 0.0f;
}

void UEnemyDetectionComponent::UpdateDetection(float DeltaTime)
{
    AEnemy* OwnerEnemy = Owner.Get();
    if (!OwnerEnemy) return;

    const float PreviousGauge = AlertGauge;
    const EEnemyAlertLevel PreviousLevel = AlertLevel;
    VisibleTarget.Reset();
    bHasLineOfSight = false;

    // 소유한 액터가 죽은 상태일 때 
    if (OwnerEnemy->IsDead())
    {
        // 경계 단계, 게이지 초기화
        AlertLevel = EEnemyAlertLevel::Normal;
        AlertGauge = 0.0f;
        // 마지막 위치 인식 여부 초기화
        bHasLastKnownLocation = false;
    }
    // 소유한 액터의 BeginPlay가 호출되고, 죽지 않은 경우
    else if (OwnerEnemy->IsReadyForAI())
    {
        // 최초 발각은 공통 규칙, 전투 중 시야는 적의 전투 정책을 따른다.
        const float SightRadius = AlertLevel == EEnemyAlertLevel::Combat
            ? OwnerEnemy->GetCombatSightRadius() : GetCurrentDetectionRadius();
        APawn* Player = FindVisibleTarget(SightRadius);

        // 플레이어가 유효하고, 플레이어가 시야 거리 안에서 보이면
        if (Player)
        {
            // 현재 감지한 타겟을 플레이어로 지정
            VisibleTarget = Player;
            // BB용 변수 세팅
            bHasLineOfSight = true;
            bHasLastKnownLocation = true;

            // 마지막으로 목격된 위치 저장
            LastKnownLocation = Player->GetActorLocation();

            // 발각: 발각 영역 안에서 적이 플레이어 쪽을 바라보고 있어야 한다. 시야(레이캐스트)는 위에서 확인했다.
            // 등 뒤에서 발각 영역에 들어오면 감지 범위와 같이 게이지만 찬다.
            const FVector ToPlayer = Player->GetActorLocation() - OwnerEnemy->GetActorLocation();
            const bool bInDiscoveryRange = ToPlayer.SizeSquared() <= FMath::Square(GetCurrentDiscoveryRadius());
            // 횡스크롤이므로 좌우(X)만 비교한다. 거의 겹쳐 있으면 방향과 무관하게 정면으로 본다.
            const bool bFacingPlayer = FMath::Abs(ToPlayer.X) <= 10.0f
                || OwnerEnemy->GetActorForwardVector().X * ToPlayer.X > 0.0f;
            if (bInDiscoveryRange && bFacingPlayer)
            {
                // 감지 게이지 가득 채움
                AlertGauge = 1.0f;
                // 감지 단계 전투로 변경
                AlertLevel = EEnemyAlertLevel::Combat;
            }
            // 감지 단계가 일반인 경우
            else if (AlertLevel == EEnemyAlertLevel::Normal)
            {
                // 감지 게이지를 DeltaTime만큼 증가. 등 뒤에서는 더 느리게 찬다.
                const float FillRate = bFacingPlayer ? 1.0f : FMath::Clamp(BehindGaugeFillRate, 0.0f, 1.0f);
                if (FillRate > 0.0f)
                {
                    AlertGauge = FMath::Clamp(AlertGauge + DeltaTime * FillRate / FMath::Max(0.01f, GaugeFillTime), 0.0f, 1.0f);
                    // 감지 게이지가 1이 넘으면 경계 단계로 격상
                    if (AlertGauge >= 1.0f) AlertLevel = EEnemyAlertLevel::Alert;
                }
                else
                {
                    // 등 뒤에서 알아채지 못하는 설정이면 보이지 않을 때와 같이 줄어든다.
                    AlertGauge = FMath::Max(0.0f, AlertGauge - DeltaTime / FMath::Max(0.01f, GaugeDecayTime));
                }
            }

            // 경계 상태는 알아차린 지점만 수색하므로 보이더라도 복귀 카운트를 계속 진행한다.
            // 정면 발각 범위에서 보이면 위에서 전투로 전환된다. 일반/전투 상태는 보이면 초기화한다.
            if (AlertLevel == EEnemyAlertLevel::Alert)
                TimeWithoutSight += DeltaTime;
            else
                TimeWithoutSight = 0.0f;
        }
        // 플레이어가 유효하지 않거나, 시야 거리 안에서 보이지 않는 경우
        else
        {
            // 시야에서 사라진 채 지난 시간 누적
            TimeWithoutSight += DeltaTime;

            // 일반 단계인 경우 게이지 감소
            if (AlertLevel == EEnemyAlertLevel::Normal)
                AlertGauge = FMath::Max(0.0f, AlertGauge - DeltaTime / FMath::Max(0.01f, GaugeDecayTime));
        }

        // 마지막 실제 관측 이후의 시간만 센다. 장애물 뒤나 시야 밖의 현재 위치는 갱신하지 않는다.
        // 설정한 초기화 기간을 넘으면 감지와 관련된 값 초기화 (일반 상태로 복귀)
        if (TimeWithoutSight >= FMath::Max(0.0f, ForgetDelay))
        {
            AlertLevel = EEnemyAlertLevel::Normal;
            AlertGauge = 0.0f;
            TimeWithoutSight = 0.0f;
            bHasLastKnownLocation = false;
            LastKnownLocation = FVector::ZeroVector;
        }
    }
	// 소리를 들은 직후에는 그때 도달한 값 아래로 내려가지 않고, 가득 차면 소리 위치를 수색하는 경계 상태가 된다.
	if (bIsNoise && AlertLevel == EEnemyAlertLevel::Normal && OwnerEnemy->IsReadyForAI())
	{
		AlertGauge = FMath::Max(AlertGauge, NoiseAlertMin);
		if (AlertGauge >= 1.0f)
		{
			AlertLevel = EEnemyAlertLevel::Alert;
			LastKnownLocation = NoiseLocation;
			bHasLastKnownLocation = true;
			TimeWithoutSight = 0.0f;
		}
	}

    UpdateBlackboard();
    // 경계 게이지와, 경계 단계가 변경되면 브로드캐스트
    if (PreviousLevel != AlertLevel || !FMath::IsNearlyEqual(PreviousGauge, AlertGauge))
        OnAwarenessChanged.Broadcast(AlertGauge, AlertLevel);
}

void UEnemyDetectionComponent::UpdateBlackboard()
{
    AEnemy* OwnerEnemy = Owner.Get();
    if (!OwnerEnemy) return;
    
    AAIController* AI = Cast<AAIController>(OwnerEnemy->GetController());
    if (!AI) return;

    UBlackboardComponent* BB = AI->GetBlackboardComponent();
    if (!BB) return; // 컨트롤러/BT가 나중에 준비되어도 다음 Tick에서 다시 연결한다.

    APawn* Target = VisibleTarget.Get();

    bool bCanFight = false;
    if (AlertLevel == EEnemyAlertLevel::Combat && bHasLineOfSight && IsValid(Target))
    {
        bCanFight = true;
    }
    // 공격 중 이동을 고정하는 정책이면 거리/시야 Decorator의 중단도 완료까지 보류한다.
    // 실제 감지 상태와 UI는 계속 갱신한다. 몽타주가 끝나면 다음 감지 갱신에서 그대로 반영된다.
    // 공격 태스크만 거리 검사를 생략해도 Decorator가 먼저 Abort할 수 있어, 관련 Blackboard 값도 함께 유지해야 한다.
    // 공격을 시작한 CombatTarget을 사용하므로 투척 준비 중 다른 대상이 보이더라도 이번 공격 대상은 바뀌지 않는다.
    const bool bCommittedAttack = OwnerEnemy->ShouldLockMovementDuringAttack() && OwnerEnemy->IsAttackActive()
        && !OwnerEnemy->IsPlayingHitReaction() && OwnerEnemy->IsValidAITarget(OwnerEnemy->GetCombatTarget());
    if (bCommittedAttack)
    {
        Target = Cast<APawn>(OwnerEnemy->GetCombatTarget());
        bCanFight = true;
    }
    // Observer가 중간 상태를 보고 잘못된 태스크를 실행하지 않도록 한 번에 알린다.
    // 값 갱신 노티파이를 잠시 정지
    BB->PauseObserverNotifications();

    // 블랙 보드 변수에 값 세팅
    BB->SetValueAsObject(EnemyDetectionKeys::TargetActor, bCanFight ? Target : nullptr);
    BB->SetValueAsBool(EnemyDetectionKeys::HasSight, bCommittedAttack || bHasLineOfSight);
    BB->SetValueAsBool(EnemyDetectionKeys::HasLastLocation, bHasLastKnownLocation);
    BB->SetValueAsBool(EnemyDetectionKeys::InAttackRange, bCommittedAttack || (bCanFight && OwnerEnemy->CanStartAIAttack(Target)));
    BB->SetValueAsBool(EnemyDetectionKeys::IsDead, OwnerEnemy->IsDead());
    BB->SetValueAsBool(EnemyDetectionKeys::IsHitReacting, !OwnerEnemy->IsDead() && OwnerEnemy->IsPlayingHitReaction());
    BB->SetValueAsEnum(EnemyDetectionKeys::AlertLevel, static_cast<uint8>(bCommittedAttack ? EEnemyAlertLevel::Combat : AlertLevel));
    
    // 마지막 목격 위치가 있으면 기록하고 
    if (bHasLastKnownLocation)
        BB->SetValueAsVector(EnemyDetectionKeys::LastLocation, LastKnownLocation);
    // 없으면 지운다
    else
        BB->ClearValue(EnemyDetectionKeys::LastLocation);
    
    // 값 갱신 노티파이 재시작
    BB->ResumeObserverNotifications(true);
}

void UEnemyDetectionComponent::CreatePresentationWidget()
{
    if (PresentationWidget) return;

    AEnemy* OwnerEnemy = Owner.Get();
    if (!OwnerEnemy) return;
    if (OwnerEnemy->IsDead()) return;

    UWorld* World = GetWorld();
    if (!World) return;

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC)
    {
        // 플레이어가 준비되지 않았을 때만 일회성 타이머로 재시도한다.
        World->GetTimerManager().SetTimer(PresentationRetryTimer, this,
            &UEnemyDetectionComponent::CreatePresentationWidget, 0.25f, false);
        return;
    }

    World->GetTimerManager().ClearTimer(PresentationRetryTimer);

    TSubclassOf<UEnemyAwarenessWidget> WidgetClass = AwarenessWidgetClass;
    if (!WidgetClass) WidgetClass = UEnemyAwarenessWidget::StaticClass();

    PresentationWidget = CreateWidget<UEnemyAwarenessWidget>(PC, WidgetClass);
    if (!PresentationWidget) return;

    PresentationWidget->InitializeDetection(this);
    PresentationWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
    PresentationWidget->ForceVolatile(true);
    PresentationWidget->AddToViewport(5);
}

void UEnemyDetectionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PresentationRetryTimer);
    }

    if (PresentationWidget)
    {
        PresentationWidget->RemoveFromParent();
        PresentationWidget = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}
