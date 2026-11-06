// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/BossCinematicDirector.h"

#include "Character/BossEnemy.h"
#include "Level/BossGimmickPortal.h"
#include "Level/TutorialSequenceTrigger.h"

#include "GAS/Tags/EclipseGameplayTags.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ABossCinematicDirector::ABossCinematicDirector()
{
    PrimaryActorTick.bCanEverTick = false;

    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
}

void ABossCinematicDirector::BeginPlay()
{
    Super::BeginPlay();

    // 다른 맵으로 옮겼을 때 보스를 다시 지정하지 않아도 되도록, 비어 있으면 맵의 보스를 찾는다
    if (!IsValid(this->Boss))
    {
        TActorIterator<ABossEnemy> It(GetWorld());
        this->Boss = It ? *It : nullptr;
    }

    if (!IsValid(this->Boss))
    {
        UE_LOG(LogTemp, Warning, TEXT("[ABossCinematicDirector::BeginPlay()] %s : 보스가 지정되지 않음"), *GetActorNameOrLabel());
        return;
    }

    this->Boss->OnEnergyLinkActivated.AddDynamic(this, &ABossCinematicDirector::HandleEnergyLinkActivated);
    this->Boss->OnEnergyLinkBroken.AddDynamic(this, &ABossCinematicDirector::HandleEnergyLinkBroken);
    this->Boss->OnExecutionStarted.AddDynamic(this, &ABossCinematicDirector::HandleExecutionStarted);

    // 보스 스테이지 쪽 포탈에 도착하는 순간 그로기를 시작한다
    for (TActorIterator<ABossGimmickPortal> It(GetWorld()); It; ++It)
    {
        if (It->GetPortalSide() == EBossGimmickPortalSide::BossStage)
        {
            It->OnPlayerArrived.AddDynamic(this, &ABossCinematicDirector::HandlePortalArrived);
        }
    }

    TArray<ATutorialSequenceTrigger*> Triggers = { this->IntroSequence, this->ExecutionSequence, this->GroggySequence };
    for (ATutorialSequenceTrigger* Trigger : this->LinkHintSequences)
    {
        Triggers.Add(Trigger);
    }
    for (ATutorialSequenceTrigger* Trigger : Triggers)
    {
        if (IsValid(Trigger))
        {
            Trigger->OnSequenceEnded.AddUniqueDynamic(this, &ABossCinematicDirector::HandleSequenceEnded);
        }
    }

    if (IsValid(this->IntroSequence))
    {
        // 진입 컷신에는 연출용 보스가 칼을 뽑으므로, 끝날 때까지 실제 보스는 숨기고 멈춰 둔다
        this->Boss->SetActorHiddenInGame(true);
        this->bHidBoss = true;

        // 보스 AI(비헤이비어 트리)가 시작된 뒤에 멈추도록 한 프레임 뒤에 재생한다
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            PauseBoss();
            this->IntroSequence->PlaySequence(UGameplayStatics::GetPlayerController(this, 0));
        }));
    }
}

void ABossCinematicDirector::HandleEnergyLinkActivated(int32 LinkIndex)
{
    this->bGroggyPending = false;

    ATutorialSequenceTrigger* Trigger = this->LinkHintSequences.IsValidIndex(LinkIndex) ? this->LinkHintSequences[LinkIndex].Get() : nullptr;
    if (!IsValid(Trigger))
    {
        return;
    }

    // 힌트는 한 번만 재생된다. 재생되지 않으면(체크포인트 재도전) 보스를 멈추지 않는다
    Trigger->PlaySequence(UGameplayStatics::GetPlayerController(this, 0));
    if (!Trigger->IsPlaying())
    {
        return;
    }

    // 힌트 동안 보스를 그 자리에 완전히 멈춘다. AI만 멈추면 이미 시작한 공격·이동이 그대로 이어진다
    PauseBoss();
    this->Boss->CustomTimeDilation = 0.0f;
}

void ABossCinematicDirector::HandleEnergyLinkBroken()
{
    // 레버는 기믹 스테이지에 있으므로 보스 스테이지로 돌아온 순간 그로기를 보여준다
    this->bGroggyPending = true;
}

void ABossCinematicDirector::HandleExecutionStarted()
{
    APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!IsValid(this->ExecutionSequence) || !IsValid(Player))
    {
        return;
    }

    // 처형 어빌리티가 보스를 플레이어 등 뒤로 옮긴 뒤라, 두 캐릭터 사이 바닥을 원점으로 쓴다
    const UCapsuleComponent* Capsule = Player->FindComponentByClass<UCapsuleComponent>();
    FVector Origin = (Player->GetActorLocation() + this->Boss->GetActorLocation()) * 0.5f;
    Origin.Z = Player->GetActorLocation().Z - (Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f);

    this->ExecutionSequence->PlaySequenceAt(UGameplayStatics::GetPlayerController(this, 0), FTransform(Origin));
}

void ABossCinematicDirector::HandlePortalArrived(ABossGimmickPortal* Portal, APawn* Player)
{
    if (!this->bGroggyPending || !IsValid(this->GroggySequence))
    {
        return;
    }
    this->bGroggyPending = false;

    PauseBoss();

    if (IsValid(this->GroggyAnimation))
    {
        if (UAnimInstance* AnimInstance = this->Boss->GetMesh()->GetAnimInstance())
        {
            this->GroggyMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
                this->GroggyAnimation, this->GroggySlot, 0.2f, GroggyBlendOutTime, this->GroggyPlayRate);

            // 쓰러지는 애니메이션이 끝나도 바로 일어서지 않게 자동 블렌드 아웃을 끈다. 몽타주는 마지막 자세로 멈춰 있고,
            // 컷신이 끝나고 GroggyHoldTime이 지나면 FinishGroggy에서 멈추며 원래 자세로 블렌드해 일어선다.
            // (끝나기 직전에 타이머로 멈추면 프레임이 낮을 때 블렌드 아웃이 먼저 시작돼 일어서 버린다)
            // 재생 중인 인스턴스는 재생 시점에 몽타주 값을 복사해 두므로 인스턴스 쪽을 끈다
            if (FAnimMontageInstance* MontageInstance = IsValid(this->GroggyMontage) ? AnimInstance->GetActiveInstanceForMontage(this->GroggyMontage) : nullptr)
            {
                MontageInstance->bEnableAutoBlendOut = false;
            }
        }
    }

    this->GroggySequence->PlaySequenceAt(Cast<APlayerController>(Player->GetController()), GetBossFloorTransform());
}

void ABossCinematicDirector::HandleSequenceEnded(ATutorialSequenceTrigger* Trigger)
{
    if (this->bHidBoss && Trigger == this->IntroSequence)
    {
        this->Boss->SetActorHiddenInGame(false);
        this->bHidBoss = false;
    }

    if (this->LinkHintSequences.Contains(Trigger))
    {
        this->Boss->CustomTimeDilation = 1.0f;
    }

    // 그로기는 컷신이 끝난 뒤에도 GroggyHoldTime 동안 쓰러진 채로 멈춰 있다가 일어선다
    if (Trigger == this->GroggySequence && this->GroggyHoldTime > 0.0f)
    {
        GetWorldTimerManager().SetTimer(this->GroggyHoldTimer, this, &ABossCinematicDirector::FinishGroggy, this->GroggyHoldTime, false);
        return;
    }

    ResumeBoss();
}

void ABossCinematicDirector::FinishGroggy()
{
    // 쓰러져 있는 동안 처치됐으면 사망 연출을 그대로 둔다
    if (!IsValid(this->Boss) || this->Boss->IsDead())
    {
        this->GroggyMontage = nullptr;
        return;
    }

    UAnimInstance* AnimInstance = this->Boss->GetMesh()->GetAnimInstance();
    if (AnimInstance && IsValid(this->GroggyMontage))
    {
        // 멈춰 둔 쓰러진 자세에서 원래 자세로 블렌드하며 일어선다
        AnimInstance->Montage_Stop(this->GroggyRecoverBlendTime, this->GroggyMontage);
    }
    this->GroggyMontage = nullptr;

    ResumeBoss();
}

void ABossCinematicDirector::PauseBoss()
{
    if (this->bPausedBoss || !IsValid(this->Boss))
    {
        return;
    }

    // 컷신 동안 어떤 경로로도 패턴을 시작하지 못하게 하고, 이미 시작한 패턴(예고·공격 몽타주)은 끊는다
    this->Boss->SetCinematicLocked(true);
    if (UAbilitySystemComponent* ASC = this->Boss->GetAbilitySystemComponent())
    {
        const FGameplayTagContainer AttackTags(EclipseGameplayTags::Ability_Attack);
        ASC->CancelAbilities(&AttackTags);
    }

    AAIController* AI = Cast<AAIController>(this->Boss->GetController());
    UBrainComponent* Brain = AI ? AI->GetBrainComponent() : nullptr;
    if (!IsValid(Brain))
    {
        return;
    }

    AI->StopMovement();
    this->Boss->GetCharacterMovement()->StopMovementImmediately();
    Brain->PauseLogic(TEXT("BossCinematic"));
    this->bPausedBoss = true;
}

void ABossCinematicDirector::ResumeBoss()
{
    if (IsValid(this->Boss))
    {
        this->Boss->SetCinematicLocked(false);
    }
    if (!this->bPausedBoss || !IsValid(this->Boss))
    {
        return;
    }
    this->bPausedBoss = false;

    AAIController* AI = Cast<AAIController>(this->Boss->GetController());
    if (UBrainComponent* Brain = AI ? AI->GetBrainComponent() : nullptr)
    {
        Brain->ResumeLogic(TEXT("BossCinematic"));
    }
}

FTransform ABossCinematicDirector::GetBossFloorTransform() const
{
    FVector Location = this->Boss->GetActorLocation();
    Location.Z -= this->Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    return FTransform(Location);
}
