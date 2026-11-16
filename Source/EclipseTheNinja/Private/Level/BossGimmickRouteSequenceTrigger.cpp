#include "Level/BossGimmickRouteSequenceTrigger.h"

#include "Character/BossEnemy.h"
#include "Component/BossEnemyGimmickComponent.h"
#include "Level/BossGimmickPortal.h"
#include "Save/GameSaveSubsystem.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"

ABossGimmickRouteSequenceTrigger::ABossGimmickRouteSequenceTrigger()
{
    // 포탈 도착으로만 재생한다. 처음 들어올 때 한 번만 보여주고, 건너뛸 수 있다
    this->bTriggerOnOverlap = false;
    this->bPlayOnce = true;
    this->bAllowSkip = true;
}

void ABossGimmickRouteSequenceTrigger::BeginPlay()
{
    Super::BeginPlay();

    this->OnSequenceEnded.AddUniqueDynamic(this, &ABossGimmickRouteSequenceTrigger::HandleRouteSequenceEnded);

    if (!IsValid(this->EntryPortal))
    {
        UE_LOG(LogTemp, Warning, TEXT("[ABossGimmickRouteSequenceTrigger::BeginPlay()] %s : EntryPortal이 비어 있어 재생하지 않음"), *GetActorNameOrLabel());
        return;
    }
    this->EntryPortal->OnPlayerArrived.AddUniqueDynamic(this, &ABossGimmickRouteSequenceTrigger::HandlePortalArrived);
}

void ABossGimmickRouteSequenceTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 재생 도중 맵을 떠나거나 기믹 방이 내려가도 링크 제한시간이 멈춘 채로 남지 않게 한다
    ResumeLinkTimer();

    if (IsValid(this->EntryPortal))
    {
        this->EntryPortal->OnPlayerArrived.RemoveDynamic(this, &ABossGimmickRouteSequenceTrigger::HandlePortalArrived);
    }

    Super::EndPlay(EndPlayReason);
}

void ABossGimmickRouteSequenceTrigger::HandlePortalArrived(ABossGimmickPortal* Portal, APawn* Player)
{
    if (Portal != this->EntryPortal || !IsValid(Player) || IsPlaying())
    {
        return;
    }

    APlayerController* PlayerController = Cast<APlayerController>(Player->GetController());
    if (!IsValid(PlayerController))
    {
        return;
    }

    // 부모는 이미 본 시퀀스만 건너뛰게 하므로, 재생 전에 본 것으로 기록해 처음부터 건너뛸 수 있게 한다
    if (this->bAllowSkip && this->bAllowSkipFirstTime)
    {
        const ULevelSequence* Sequence = GetSequenceAsset();
        UGameSaveSubsystem* SaveSubsystem = UGameSaveSubsystem::Get(this);
        if (Sequence && SaveSubsystem && !SaveSubsystem->HasSeenSequence(Sequence->GetPackage()->GetFName()))
        {
            SaveSubsystem->MarkSequenceSeen(Sequence->GetPackage()->GetFName());
        }
    }

    // 재생을 시작하기 전에 멈춰, 컷신 첫 프레임부터 시간이 줄지 않게 한다
    PauseLinkTimer();
    PlaySequence(PlayerController);

    // Play Once로 이미 본 뒤라 재생되지 않았으면 바로 되돌린다
    if (!IsPlaying())
    {
        ResumeLinkTimer();
    }
}

void ABossGimmickRouteSequenceTrigger::HandleRouteSequenceEnded(ATutorialSequenceTrigger* Trigger)
{
    ResumeLinkTimer();
}

ABossEnemy* ABossGimmickRouteSequenceTrigger::FindLinkedBoss() const
{
    if (!IsValid(this->EntryPortal))
    {
        return nullptr;
    }
    const UBossEnemyGimmickComponent* Gimmick = UBossEnemyGimmickComponent::FindByLinkId(this, this->EntryPortal->GetLinkId());
    return Gimmick ? Cast<ABossEnemy>(Gimmick->GetOwner()) : nullptr;
}

void ABossGimmickRouteSequenceTrigger::PauseLinkTimer()
{
    if (this->PausedBoss.IsValid())
    {
        return;
    }

    ABossEnemy* Boss = FindLinkedBoss();
    if (!IsValid(Boss))
    {
        UE_LOG(LogTemp, Warning, TEXT("[ABossGimmickRouteSequenceTrigger::PauseLinkTimer()] %s : 링크 %s의 보스를 찾지 못해 제한시간을 멈추지 않음"),
            *GetActorNameOrLabel(), *this->EntryPortal->GetLinkId().ToString());
        return;
    }

    Boss->SetEnergyLinkTimerPaused(true);
    this->PausedBoss = Boss;
}

void ABossGimmickRouteSequenceTrigger::ResumeLinkTimer()
{
    if (ABossEnemy* Boss = this->PausedBoss.Get())
    {
        Boss->SetEnergyLinkTimerPaused(false);
    }
    this->PausedBoss.Reset();
}
