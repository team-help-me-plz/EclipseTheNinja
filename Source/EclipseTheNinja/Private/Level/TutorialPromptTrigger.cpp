// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/TutorialPromptTrigger.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Subsystem/UIMessageSubsystem.h"
#include "TimerManager.h"

ATutorialPromptTrigger::ATutorialPromptTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    this->SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(this->SceneRoot);

    this->PromptTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("PromptTrigger"));
    this->PromptTrigger->SetupAttachment(this->SceneRoot);
    this->PromptTrigger->SetBoxExtent(FVector(100.0f, 200.0f, 150.0f));
    this->PromptTrigger->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
    this->PromptTrigger->SetCollisionProfileName(TEXT("Trigger"));
    this->PromptTrigger->ShapeColor = FColor::Cyan;
}

void ATutorialPromptTrigger::BeginPlay()
{
    Super::BeginPlay();

    // 벽 붙기 트레이스(WallAttach)가 트리거 상자에 막히지 않게 한다. Trigger 프로필에는 WallAttach 채널이 없어 기본값 Block이 적용되므로 런타임에 지정한다.
    this->PromptTrigger->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);

    this->PromptTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATutorialPromptTrigger::OnTriggerBeginOverlap);
}

void ATutorialPromptTrigger::OnTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    // 분신 등 AI 폰은 무시하고 플레이어가 조종하는 폰만 처리
    const APawn* Pawn = Cast<APawn>(OtherActor);
    if (!IsValid(Pawn) || !Pawn->IsPlayerControlled())
    {
        return;
    }

    ShowPrompt(Cast<APlayerController>(Pawn->GetController()));
}

void ATutorialPromptTrigger::ShowPrompt(APlayerController* PlayerController)
{
    if (!PlayerController || (this->bShowOnce && this->bHasShown))
    {
        return;
    }

    // 컷신 재생 중(TutorialSequenceTrigger가 폰 입력을 막아 둔 동안)에는 문구가 가려지므로 조작이 돌아올 때까지 미룬다
    const APawn* PlayerPawn = PlayerController->GetPawn();
    if (IsValid(PlayerPawn) && !PlayerPawn->InputEnabled())
    {
        FTimerDelegate RetryDelegate = FTimerDelegate::CreateUObject(this, &ATutorialPromptTrigger::ShowPrompt, PlayerController);
        GetWorldTimerManager().SetTimer(this->DeferredPromptTimer, RetryDelegate, 0.25f, false);
        return;
    }
    this->bHasShown = true;

    UE_LOG(LogTemp, Log, TEXT("[ATutorialPromptTrigger::ShowPrompt()] %s : 안내 문구 %d개 표시"),
        *GetActorNameOrLabel(), this->Messages.Num());

    for (const FText& Message : this->Messages)
    {
        UUIMessageSubsystem::ShowMessage(PlayerController, Message, this->MessageHoldDuration);
    }
}
