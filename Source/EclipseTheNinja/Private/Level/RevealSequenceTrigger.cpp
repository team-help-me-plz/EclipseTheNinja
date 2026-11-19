#include "Level/RevealSequenceTrigger.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "TimerManager.h"

ARevealSequenceTrigger::ARevealSequenceTrigger()
{
    // 스위치가 작동시킬 때만 재생한다. 처음 한 번만 보여 주고, 이미 본 시퀀스는 건너뛸 수 있다
    this->bTriggerOnOverlap = false;
    this->bPlayOnce = true;
    this->bAllowSkip = true;
}

void ARevealSequenceTrigger::BeginPlay()
{
    Super::BeginPlay();
    this->OnSequenceEnded.AddUniqueDynamic(this, &ARevealSequenceTrigger::HandleRevealSequenceEnded);
}

void ARevealSequenceTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(this->RevealTimer);
    Super::EndPlay(EndPlayReason);
}

void ARevealSequenceTrigger::ToggleActivate_Implementation()
{
    // 재생 중에 또 작동 요청이 오면 대상 작동 뒤에 이어서 처리한다
    if (this->bRevealPending)
    {
        ++this->QueuedToggles;
        return;
    }

    // 스위치를 작동시킨 폰이 플레이어면 그 컨트롤러로, 분신 등이면 첫 플레이어 컨트롤러로 재생한다
    APlayerController* PlayerController = nullptr;
    if (const APawn* Activator = GetInstigator())
    {
        PlayerController = Cast<APlayerController>(Activator->GetController());
    }
    if (!IsValid(PlayerController))
    {
        PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    }

    PlaySequence(PlayerController);

    // Play Once로 이미 재생했거나 시퀀스가 없으면 재생되지 않는다. 이때는 원래 스위치처럼 바로 작동시킨다
    if (!IsPlaying())
    {
        ToggleRevealTargets();
        return;
    }

    this->bRevealPending = true;
    this->QueuedToggles = 0;
    GetWorldTimerManager().SetTimer(this->RevealTimer, this, &ARevealSequenceTrigger::CheckRevealFrame, 0.02f, true);
    CheckRevealFrame();
}

void ARevealSequenceTrigger::CheckRevealFrame()
{
    const ULevelSequencePlayer* SequencePlayer = IsValid(this->SequenceActor) ? this->SequenceActor->GetSequencePlayer() : nullptr;
    if (!SequencePlayer)
    {
        Reveal();
        return;
    }

    // 건너뛰기로 끝 프레임에 넘어가도 여기서 도달로 본다
    if (SequencePlayer->GetCurrentTime().Time.FrameNumber.Value >= this->RevealFrame)
    {
        Reveal();
    }
}

void ARevealSequenceTrigger::HandleRevealSequenceEnded(ATutorialSequenceTrigger* Trigger)
{
    Reveal();
}

void ARevealSequenceTrigger::Reveal()
{
    if (!this->bRevealPending)
    {
        return;
    }
    this->bRevealPending = false;
    GetWorldTimerManager().ClearTimer(this->RevealTimer);

    ToggleRevealTargets();
    for (int32 Index = 0; Index < this->QueuedToggles; ++Index)
    {
        ToggleRevealTargets();
    }
    this->QueuedToggles = 0;

    UE_LOG(LogTemp, Log, TEXT("[ARevealSequenceTrigger::Reveal()] %s : 대상 %d개 작동"), *GetActorNameOrLabel(), this->RevealTargets.Num());
}

void ARevealSequenceTrigger::ToggleRevealTargets()
{
    APawn* Activator = GetInstigator();
    for (AActor* Target : this->RevealTargets)
    {
        if (!IsValid(Target) || !Target->Implements<UActivatable>())
        {
            continue;
        }
        if (Activator)
        {
            Target->SetInstigator(Activator);
        }
        IActivatable::Execute_ToggleActivate(Target);
    }
}

void ARevealSequenceTrigger::ShowActivationIndicator_Implementation()
{
    for (AActor* Target : this->RevealTargets)
    {
        if (IsValid(Target) && Target->Implements<UActivatable>())
        {
            IActivatable::Execute_ShowActivationIndicator(Target);
        }
    }
}

void ARevealSequenceTrigger::HideActivationIndicator_Implementation()
{
    for (AActor* Target : this->RevealTargets)
    {
        if (IsValid(Target) && Target->Implements<UActivatable>())
        {
            IActivatable::Execute_HideActivationIndicator(Target);
        }
    }
}
