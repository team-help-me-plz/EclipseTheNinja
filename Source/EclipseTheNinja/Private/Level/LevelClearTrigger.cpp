// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/LevelClearTrigger.h"
#include "Save/GameSaveSubsystem.h"
#include "Engine/GameInstance.h"

#include "Character/BossEnemy.h"
#include "Level/TutorialSequenceTrigger.h"
#include "LevelSequence.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "Widgets/GameEndCountdownWidget.h"

ALevelClearTrigger::ALevelClearTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    this->SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(this->SceneRoot);

    this->ClearTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ClearTrigger"));
    this->ClearTrigger->SetupAttachment(this->SceneRoot);
    this->ClearTrigger->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
    this->ClearTrigger->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
    this->ClearTrigger->SetCollisionProfileName(TEXT("Trigger"));
    this->ClearTrigger->SetHiddenInGame(false);
    this->ClearTrigger->ShapeColor = FColor::Yellow;
}

void ALevelClearTrigger::BeginPlay()
{
    Super::BeginPlay();

    // 벽 붙기 트레이스(WallAttach)가 트리거 상자에 막히지 않게 한다. Trigger 프로필에는 WallAttach 채널이 없어 기본값 Block이 적용되므로 런타임에 지정한다.
    this->ClearTrigger->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);

    if (IsValid(this->BossToDefeat))
    {
        // 보스전은 출구에 먼저 도착해도 클리어할 수 없다.
        this->ClearTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        this->ClearTrigger->SetGenerateOverlapEvents(false);
        this->BossToDefeat->OnDeathFinished.AddUObject(this, &ALevelClearTrigger::OnBossDeathFinished);
        this->BossToDefeat->OnDied.AddUObject(this, &ALevelClearTrigger::OnBossDied);
    }
    else
    {
        this->ClearTrigger->OnComponentBeginOverlap.AddDynamic(this, &ALevelClearTrigger::OnTriggerBeginOverlap);
    }
}

void ALevelClearTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 카운트다운 도중 월드가 내려가면 티커가 남지 않도록 해제
    if (IsValid(this->BossToDefeat))
    {
        this->BossToDefeat->OnDeathFinished.RemoveAll(this);
        this->BossToDefeat->OnDied.RemoveAll(this);
    }

    GetWorldTimerManager().ClearTimer(this->TravelTimerHandle);
    if (this->QuitTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(this->QuitTickerHandle);
        this->QuitTickerHandle.Reset();
    }

    Super::EndPlay(EndPlayReason);
}

void ALevelClearTrigger::OnBossDeathFinished(AEnemy* Enemy)
{
    if (Enemy == this->BossToDefeat && IsValid(Enemy) && Enemy->IsDead())
    {
        ClearLevel();
    }
}

void ALevelClearTrigger::OnBossDied(AEnemy* Enemy, bool bAssassinated)
{
    const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (Enemy == this->BossToDefeat && IsValid(PlayerPawn))
    {
        this->bHasDefeatFacing = true;
        this->bDefeatFacingLeft = Enemy->GetActorLocation().X < PlayerPawn->GetActorLocation().X;
    }
}

void ALevelClearTrigger::OnTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (IsValid(this->BossToDefeat))
    {
        return;
    }

    // 분신 등 AI 폰은 무시하고 플레이어가 조종하는 폰만 클리어 처리
    const APawn* Pawn = Cast<APawn>(OtherActor);
    if (!IsValid(Pawn) || !Pawn->IsPlayerControlled())
    {
        return;
    }

    ClearLevel();
}

void ALevelClearTrigger::ClearLevel()
{
    if (this->bIsCleared)
    {
        return;
    }
    this->bIsCleared = true;

    GetGameInstance()->GetSubsystem<UGameSaveSubsystem>()->SaveStageClear(this->NextLevel, this->bQuitGameOnClear || this->bFinalStage, this->ArrivalCheckpoint, this->bMarkLevelCleared);

    UE_LOG(LogTemp, Log, TEXT("[ALevelClearTrigger::ClearLevel()] 레벨 클리어 : %s -> %s"),
        *UGameplayStatics::GetCurrentLevelName(this),
        this->bQuitGameOnClear ? TEXT("게임 종료") : *this->NextLevel.ToString());

    // 이동 대기 중 플레이어가 움직이지 않도록 입력 차단
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (PlayerController)
    {
        if (APawn* PlayerPawn = PlayerController->GetPawn())
        {
            PlayerPawn->DisableInput(PlayerController);

        }
    }

    this->OnLevelCleared.Broadcast();

    // 엔딩 컷신을 먼저 재생하고, 끝 프레임에 도달하면 이동·종료를 이어 간다
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (IsValid(this->EndingSequence) && IsValid(PlayerPawn))
    {
        // 시퀀스는 플레이어가 다가오는 쪽 시체 끝(바닥 높이)을 원점으로, 플레이어가 시체 왼쪽(-X)에서 걸어와
        // 시체 앞에서 납도·집중한 뒤 연기와 함께 사라지는 기준이다. 시체는 시퀀스에서 원점의 +X쪽에 놓인다.
        // 보스가 없으면 플레이어 발밑을 원점으로 쓴다
        FVector Origin = PlayerPawn->GetActorLocation();
        if (const UCapsuleComponent* Capsule = PlayerPawn->FindComponentByClass<UCapsuleComponent>())
        {
            Origin.Z -= Capsule->GetScaledCapsuleHalfHeight();
        }
        if (IsValid(this->BossToDefeat))
        {
            Origin = this->BossToDefeat->GetActorLocation();
            Origin.Z -= this->BossToDefeat->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        }

        float EndingStartTime = 0.0f;
        if (IsValid(this->EndingSequenceFacingLeft))
        {
            if (!IsValid(this->EndingSequenceFacingRight))
            {
                this->EndingSequenceFacingRight = this->EndingSequence->GetSequenceAsset();
            }

            // 플레이어는 시체에서 항상 같은 거리(EndingApproachDistance)부터 걸어오고, 벽이나 낭떠러지가 없는 쪽에서 온다.
            // 양쪽 다 트여 있으면 지금 플레이어가 서 있는 쪽에서 오고, 한쪽만 트여 있으면 그쪽에서 온다
            bool bApproachFromRight = false;
            if (IsValid(this->BossToDefeat))
            {
                // 시체는 보스가 바라보던 방향의 뒤쪽으로 누워 있다. 원점을 플레이어가 다가오는 쪽 시체 끝으로 옮겨,
                // 플레이어가 시체를 밟지 않고 그 끝에서 EndingStopDistance만큼 떨어져 서게 한다
                const float BackSign = this->BossToDefeat->GetActorForwardVector().X >= 0.0f ? -1.0f : 1.0f;
                const float CorpseMinX = FMath::Min(BackSign * this->EndingCorpseNearOffset, BackSign * this->EndingCorpseFarOffset);
                const float CorpseMaxX = FMath::Max(BackSign * this->EndingCorpseNearOffset, BackSign * this->EndingCorpseFarOffset);
                const FVector LeftOrigin = Origin + FVector(CorpseMinX, 0.0f, 0.0f);
                const FVector RightOrigin = Origin + FVector(CorpseMaxX, 0.0f, 0.0f);

                const float LeftClearance = GetEndingApproachClearance(PlayerPawn, LeftOrigin, FVector(-1.0f, 0.0f, 0.0f));
                const float RightClearance = GetEndingApproachClearance(PlayerPawn, RightOrigin, FVector(1.0f, 0.0f, 0.0f));
                const bool bLeftOpen = LeftClearance >= this->EndingApproachDistance;
                const bool bRightOpen = RightClearance >= this->EndingApproachDistance;
                if (bLeftOpen && bRightOpen)
                {
                    bApproachFromRight = PlayerPawn->GetActorLocation().X > Origin.X;
                }
                else if (bLeftOpen || bRightOpen)
                {
                    bApproachFromRight = bRightOpen;
                }
                else
                {
                    bApproachFromRight = RightClearance > LeftClearance;
                }
                Origin = bApproachFromRight ? RightOrigin : LeftOrigin;

                // 양쪽 다 거리가 모자랄 때만 걷기 앞부분을 건너뛰어, 막힌 곳 안쪽에서 걷기 시작하게 한다
                const float Clearance = bApproachFromRight ? RightClearance : LeftClearance;
                const float WalkLength = this->EndingApproachDistance - this->EndingStopDistance;
                if (WalkLength > KINDA_SMALL_NUMBER && Clearance < this->EndingApproachDistance)
                {
                    const float StartDistance = FMath::Clamp(Clearance, this->EndingStopDistance, this->EndingApproachDistance);
                    EndingStartTime = (this->EndingApproachDistance - StartDistance) / WalkLength * this->EndingWalkDuration;
                }
                UE_LOG(LogTemp, Log, TEXT("[ALevelClearTrigger::ClearLevel()] 엔딩 걸어올 길 : 왼쪽 %.0f, 오른쪽 %.0f -> %s에서 걸어옴, %.2f초부터 재생"),
                    LeftClearance, RightClearance, bApproachFromRight ? TEXT("오른쪽") : TEXT("왼쪽"), EndingStartTime);
            }
            else
            {
                bApproachFromRight = this->bHasDefeatFacing ? this->bDefeatFacingLeft : PlayerPawn->GetActorForwardVector().X < 0.0;
            }
            this->EndingSequence->SetSequenceAsset(bApproachFromRight ? this->EndingSequenceFacingLeft.Get() : this->EndingSequenceFacingRight.Get());
        }

        this->EndingSequence->OnSequenceEnded.AddUniqueDynamic(this, &ALevelClearTrigger::OnEndingSequenceEnded);
        this->EndingSequence->PlaySequenceAt(PlayerController, FTransform(Origin), EndingStartTime);
        if (this->EndingSequence->IsPlaying())
        {
            return;
        }

        UE_LOG(LogTemp, Warning, TEXT("[ALevelClearTrigger::ClearLevel()] %s : 엔딩 컷신을 재생하지 못해 바로 넘어갑니다."), *GetActorNameOrLabel());
        this->EndingSequence->OnSequenceEnded.RemoveDynamic(this, &ALevelClearTrigger::OnEndingSequenceEnded);
    }

    FinishClear();
}

float ALevelClearTrigger::GetEndingApproachClearance(const APawn* PlayerPawn, const FVector& Origin, const FVector& Direction) const
{
    const UCapsuleComponent* Capsule = PlayerPawn->FindComponentByClass<UCapsuleComponent>();
    UWorld* World = GetWorld();
    if (!Capsule || !World)
    {
        return this->EndingApproachDistance;
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(EndingApproach), false, PlayerPawn);
    Params.AddIgnoredActor(this->BossToDefeat);
    FCollisionResponseParams ResponseParams;
    ResponseParams.CollisionResponse = Capsule->GetCollisionResponseToChannels();
    const ECollisionChannel Channel = Capsule->GetCollisionObjectType();

    // 시체 자리에서 걸어올 방향으로 플레이어 캡슐을 밀어 벽까지의 거리를 잰다. 바닥에 걸리지 않게 조금 띄운다
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const FVector Start = Origin + FVector(0.0f, 0.0f, HalfHeight + 5.0f);
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), HalfHeight);
    float Clearance = this->EndingApproachDistance;
    FHitResult WallHit;
    if (World->SweepSingleByChannel(WallHit, Start, Start + Direction * this->EndingApproachDistance, FQuat::Identity,
        Channel, Shape, Params, ResponseParams))
    {
        Clearance = WallHit.bStartPenetrating ? 0.0f : WallHit.Distance;
    }

    // 걸어올 길에 바닥이 끊기는 곳(발판 가장자리)이 있으면 거기까지만 걸어올 수 있다
    constexpr float FloorStep = 25.0f;
    for (float Distance = FloorStep; Distance <= Clearance; Distance += FloorStep)
    {
        const FVector Probe = Origin + Direction * Distance;
        FHitResult FloorHit;
        if (!World->LineTraceSingleByChannel(FloorHit, Probe + FVector(0.0f, 0.0f, 20.0f), Probe - FVector(0.0f, 0.0f, 40.0f),
            Channel, Params, ResponseParams))
        {
            return Distance - FloorStep;
        }
    }
    return Clearance;
}

void ALevelClearTrigger::OnEndingSequenceEnded(ATutorialSequenceTrigger* Trigger)
{
    Trigger->OnSequenceEnded.RemoveDynamic(this, &ALevelClearTrigger::OnEndingSequenceEnded);
    FinishClear();
}

void ALevelClearTrigger::FinishClear()
{
    if (this->bQuitGameOnClear)
    {
        this->bCountdownTravels = false;
        StartQuitCountdown(UGameplayStatics::GetPlayerController(this, 0));
        return;
    }

    if (this->NextLevel.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ALevelClearTrigger::FinishClear()] %s 의 NextLevel이 비어 있어 레벨을 이동하지 않습니다."),
            *GetActorNameOrLabel());
        return;
    }

    // 마지막 스테이지는 게임 종료 때와 같은 카운트다운 UI를 보여 준 뒤 다음 레벨(메인 메뉴)로 이동한다
    if (this->bFinalStage)
    {
        this->bCountdownTravels = true;
        StartQuitCountdown(UGameplayStatics::GetPlayerController(this, 0));
        return;
    }

    if (this->TravelDelay > 0.0f)
    {
        GetWorldTimerManager().SetTimer(this->TravelTimerHandle, this, &ALevelClearTrigger::TravelToNextLevel, this->TravelDelay, false);
    }
    else
    {
        TravelToNextLevel();
    }
}

void ALevelClearTrigger::TravelToNextLevel()
{
    // 도착 체크포인트를 옵션으로도 넘긴다. 세이브 진행이 없을 때(에디터에서 맵을 바로 실행) 다음 맵이 이 값으로 위치를 잡는다
    FString Options = this->TravelOptions;
    if (this->ArrivalCheckpoint != ECheckpointId::None)
    {
        // OpenLevel이 옵션 앞에 '?'를 붙이므로 첫 옵션에는 '?'를 붙이지 않는다
        Options += FString::Printf(TEXT("%sArrival=%s"), Options.IsEmpty() ? TEXT("") : TEXT("?"),
            *StaticEnum<ECheckpointId>()->GetNameStringByValue(static_cast<int64>(this->ArrivalCheckpoint)));
    }

    UGameplayStatics::OpenLevelBySoftObjectPtr(this, this->NextLevel, true, Options);
}

void ALevelClearTrigger::StartQuitCountdown(APlayerController* PlayerController)
{
    this->RemainingQuitSeconds = this->QuitCountdownSeconds;

    // 안내 메시지와 남은 초를 화면 중앙에 표시
    if (PlayerController)
    {
        this->CountdownWidget = CreateWidget<UGameEndCountdownWidget>(
            PlayerController,
            UGameEndCountdownWidget::StaticClass()
        );
    }

    if (this->CountdownWidget)
    {
        const FText& MessageFormat = this->bCountdownTravels ? this->TravelMessageFormat : this->QuitMessageFormat;
        this->CountdownWidget->SetMessage(FText::Format(MessageFormat, this->QuitCountdownSeconds));
        this->CountdownWidget->SetCount(this->RemainingQuitSeconds);
        this->CountdownWidget->AddToViewport(100);
    }

    // 카운트다운 동안 적·함정 등 월드가 멈추도록 일시정지
    if (this->bPauseGameOnQuitCountdown)
    {
        UGameplayStatics::SetGamePaused(this, true);
    }

    // 일시정지 중에는 월드 타이머가 멈추므로 실제 시간 기준인 코어 티커로 1초마다 센다
    this->QuitTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &ALevelClearTrigger::TickQuitCountdown),
        1.0f
    );
}

bool ALevelClearTrigger::TickQuitCountdown(float DeltaTime)
{
    --this->RemainingQuitSeconds;

    if (this->RemainingQuitSeconds <= 0)
    {
        this->QuitTickerHandle.Reset();
        if (this->bCountdownTravels)
        {
            // 일시정지를 풀고 다음 레벨로 이동한다
            if (this->CountdownWidget)
            {
                this->CountdownWidget->RemoveFromParent();
            }
            UGameplayStatics::SetGamePaused(this, false);
            TravelToNextLevel();
        }
        else
        {
            QuitGame();
        }

        // false를 반환하면 티커가 제거된다
        return false;
    }

    if (this->CountdownWidget)
    {
        this->CountdownWidget->SetCount(this->RemainingQuitSeconds);
    }

    return true;
}

void ALevelClearTrigger::QuitGame()
{
    UE_LOG(LogTemp, Log, TEXT("[ALevelClearTrigger::QuitGame()] 카운트다운 종료, 게임을 종료합니다."));

    // PIE에서는 플레이 세션이 끝나고, 패키징 빌드에서는 프로그램이 종료된다
    UKismetSystemLibrary::QuitGame(
        this,
        UGameplayStatics::GetPlayerController(this, 0),
        EQuitPreference::Quit,
        false
    );
}
