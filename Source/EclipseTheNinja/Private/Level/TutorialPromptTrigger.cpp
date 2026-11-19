// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/TutorialPromptTrigger.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PuzzelObject/BaseDoor.h"
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

    // 플레이어는 캡슐과 상호작용 박스가 함께 겹쳐 한 번 들어와도 오버랩이 여러 번 온다. 몸체(루트 캡슐)만 받는다
    // 메시는 이 트리거의 오브젝트 타입(WorldDynamic)을 무시하도록 설정돼 있어 오버랩이 오지 않는다
    if (OtherComp != Pawn->GetRootComponent())
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

    // 조건 문이 이미 열려 있으면 띄우지 않는다. 미뤄 둔 표시도 그사이 문이 열렸으면 취소된다
    if (IsValid(this->ShowOnlyWhileDoorClosed) && this->ShowOnlyWhileDoorClosed->IsOpen())
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

    // 문구가 떠 있는 동안(MessageHoldDuration) 다시 들어와도 같은 문구를 겹쳐 띄우지 않는다
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < this->NextShowTime)
    {
        return;
    }
    this->NextShowTime = Now + this->MessageHoldDuration;
    this->bHasShown = true;

    UE_LOG(LogTemp, Log, TEXT("[ATutorialPromptTrigger::ShowPrompt()] %s : 안내 문구 %d개 표시"),
        *GetActorNameOrLabel(), this->Messages.Num());

    for (const FText& Message : this->Messages)
    {
        UUIMessageSubsystem::ShowMessage(PlayerController, Message, this->MessageHoldDuration);
    }
}
