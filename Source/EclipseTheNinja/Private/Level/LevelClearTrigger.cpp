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

    GetGameInstance()->GetSubsystem<UGameSaveSubsystem>()->SaveStageClear(this->NextLevel, this->bQuitGameOnClear);

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
        FVector Origin = PlayerPawn->GetActorLocation();
        if (const UCapsuleComponent* Capsule = PlayerPawn->FindComponentByClass<UCapsuleComponent>())
        {
            Origin.Z -= Capsule->GetScaledCapsuleHalfHeight();
        }

        // 시퀀스는 오른쪽(+X)의 보스 시체를 바라보며 납도한 뒤 왼쪽(-X)으로 뒤돌아 퇴장하는 기준이다.
        // 컷신 시작 시점에 보스 시체가 플레이어보다 왼쪽(-X)에 있으면 좌우 대칭본을 재생해,
        // 항상 보스 시체를 먼저 바라본 뒤 반대쪽으로 뒤돌아 퇴장하게 한다
        if (IsValid(this->EndingSequenceFacingLeft))
        {
            if (!IsValid(this->EndingSequenceFacingRight))
            {
                this->EndingSequenceFacingRight = this->EndingSequence->GetSequenceAsset();
            }
            bool bCorpseOnLeft = false;
            if (IsValid(this->BossToDefeat))
            {
                const float DeltaX = this->BossToDefeat->GetActorLocation().X - PlayerPawn->GetActorLocation().X;
                bCorpseOnLeft = FMath::IsNearlyZero(DeltaX, 1.0f)
                    ? (this->bHasDefeatFacing ? this->bDefeatFacingLeft : PlayerPawn->GetActorForwardVector().X < 0.0)
                    : (DeltaX < 0.0f);
            }
            else
            {
                bCorpseOnLeft = this->bHasDefeatFacing ? this->bDefeatFacingLeft : PlayerPawn->GetActorForwardVector().X < 0.0;
            }
            this->EndingSequence->SetSequenceAsset(bCorpseOnLeft ? this->EndingSequenceFacingLeft.Get() : this->EndingSequenceFacingRight.Get());
        }

        this->EndingSequence->OnSequenceEnded.AddUniqueDynamic(this, &ALevelClearTrigger::OnEndingSequenceEnded);
        this->EndingSequence->PlaySequenceAt(PlayerController, FTransform(Origin));
        if (this->EndingSequence->IsPlaying())
        {
            return;
        }

        UE_LOG(LogTemp, Warning, TEXT("[ALevelClearTrigger::ClearLevel()] %s : 엔딩 컷신을 재생하지 못해 바로 넘어갑니다."), *GetActorNameOrLabel());
        this->EndingSequence->OnSequenceEnded.RemoveDynamic(this, &ALevelClearTrigger::OnEndingSequenceEnded);
    }

    FinishClear();
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
        StartQuitCountdown(UGameplayStatics::GetPlayerController(this, 0));
        return;
    }

    if (this->NextLevel.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ALevelClearTrigger::FinishClear()] %s 의 NextLevel이 비어 있어 레벨을 이동하지 않습니다."),
            *GetActorNameOrLabel());
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
    UGameplayStatics::OpenLevelBySoftObjectPtr(this, this->NextLevel, true, this->TravelOptions);
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
        this->CountdownWidget->SetMessage(FText::Format(this->QuitMessageFormat, this->QuitCountdownSeconds));
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
        QuitGame();

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
