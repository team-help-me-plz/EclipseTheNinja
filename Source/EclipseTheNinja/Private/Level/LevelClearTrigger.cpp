// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/LevelClearTrigger.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

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

    this->ClearTrigger->OnComponentBeginOverlap.AddDynamic(this, &ALevelClearTrigger::OnTriggerBeginOverlap);
}

void ALevelClearTrigger::OnTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
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

    UE_LOG(LogTemp, Log, TEXT("[ALevelClearTrigger::ClearLevel()] 레벨 클리어 : %s -> %s"),
        *UGameplayStatics::GetCurrentLevelName(this), *this->NextLevel.ToString());

    // 이동 대기 중 플레이어가 움직이지 않도록 입력 차단
    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (APawn* PlayerPawn = PlayerController->GetPawn())
        {
            PlayerPawn->DisableInput(PlayerController);
        }
    }

    this->OnLevelCleared.Broadcast();

    if (this->NextLevel.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ALevelClearTrigger::ClearLevel()] %s 의 NextLevel이 비어 있어 레벨을 이동하지 않습니다."),
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
    UGameplayStatics::OpenLevelBySoftObjectPtr(this, this->NextLevel);
}
