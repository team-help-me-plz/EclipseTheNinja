#include "GAS/Ability/GAParry.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Character/Enemy.h"
#include "Character/PlayerCharacter.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Ability/GASwiftStrike.h"
#include "GAS/Ability/GAWallAttach.h"
#include "GAS/Ability/GAWallSlide.h"
#include "GAS/Ability/GAAssassinateBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/Tags/EclipseGameplayTags.h"
#include "GAS/StatAttributeSet.h"

UGAParry::UGAParry()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
    FGameplayTagContainer Tags(EclipseGameplayTags::Ability_Parry);
    SetAssetTags(Tags);

    ActivationOwnedTags.AddTag(EclipseGameplayTags::State_Parrying);
    ActivationBlockedTags.AddTag(EclipseGameplayTags::State_Parrying);
    ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());
    BlockAbilitiesWithTag.AddTag(EclipseGameplayTags::Ability_Attack);
    ActivationBlockedTags.AddTag(UGASwiftStrike::GetSwiftStrikingTag());
    ActivationBlockedTags.AddTag(UGAWallAttach::GetWallStateTag());
    ActivationBlockedTags.AddTag(UGAWallSlide::GetWallSlideAbilityTag());
    ActivationBlockedTags.AddTag(UGAAssassinateBase::GetAssassinatingTag());
    BlockAbilitiesWithTag.AddTag(UGAWallAttach::GetWallAttachAbilityTag());
}

bool UGAParry::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;

    const APlayerCharacter* Player = ActorInfo ? Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (!IsValid(Player)) return false;

    const UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
    if (!Movement) return false;
    if (Movement->MovementMode == MOVE_None) return false;

    const UStatAttributeSet* Stats = Player->GetStatAttribute();
    if (!Stats) return false;
    if (Stats->GetHealth() <= 0.0f) return false;
    return true;
}

void UGAParry::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
        return;
    }

    bSucceeded = false;
    bRecoveryStarted = false;
    bWindowOpen = true;

    APlayerCharacter* Player = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
    if (!IsValid(Player))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
        return;
    }

    Player->SetParryMovementLocked(true);

    UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EclipseGameplayTags::Event_Parry_Success, nullptr, true, true);
    EventTask->EventReceived.AddDynamic(this, &UGAParry::HandleSuccess);
    EventTask->ReadyForActivation();

    GetAbilitySystemComponentFromActorInfo()->AddLooseGameplayTag(EclipseGameplayTags::State_ParryWindow);
    if (!IsActive()) return;

    if (ParryMontage)
    {
        // 블렌드 아웃 중에도 판정이 살아 있으므로 그 구간의 중단 알림도 받는다.
        MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, ParryMontage, 1.0f, NAME_None, true, 1.0f, 0.0f, true);
        MontageTask->OnCompleted.AddDynamic(this, &UGAParry::StartRecovery);
        MontageTask->OnInterrupted.AddDynamic(this, &UGAParry::HandleMontageInterrupted);
        MontageTask->OnCancelled.AddDynamic(this, &UGAParry::HandleMontageInterrupted);
        MontageTask->ReadyForActivation();
        return;
    }

    // 몽타주가 없을 때는 기존 시간 기반 판정을 사용한다.
    WindowTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(0.01f, WindowDuration));
    WindowTask->OnFinish.AddDynamic(this, &UGAParry::StartRecovery);
    WindowTask->ReadyForActivation();
}

void UGAParry::CloseWindow()
{
    if (!bWindowOpen) return;

    bWindowOpen = false;
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC) return;

    ASC->RemoveLooseGameplayTag(EclipseGameplayTags::State_ParryWindow);
}

void UGAParry::StartRecovery()
{
    if (!IsActive()) return;
    if (bRecoveryStarted) return;

    bRecoveryStarted = true;
    CloseWindow();
    WindowTask = nullptr;

    // 판정 동작 또는 성공 동작을 마친 뒤부터 후딜을 센다.
    UAbilityTask_WaitDelay* RecoveryTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(0.01f, RecoveryDuration));
    RecoveryTask->OnFinish.AddDynamic(this, &UGAParry::FinishRecovery);
    RecoveryTask->ReadyForActivation();
}

void UGAParry::FinishRecovery()
{
    if (!IsActive()) return;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGAParry::HandleMontageInterrupted()
{
    if (!IsActive()) return;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

void UGAParry::HandleSuccess(FGameplayEventData Payload)
{
    if (!IsActive()) return;
    if (!bWindowOpen) return;
    if (bSucceeded) return;

    bSucceeded = true;
    CloseWindow();

    // 성공 동작을 재생하는 동안 예전 판정 타이머가 후딜을 시작하지 않게 한다.
    if (WindowTask)
    {
        WindowTask->EndTask();
        WindowTask = nullptr;
    }

    AActor* Attacker = const_cast<AActor*>(Payload.Instigator.Get());
    if (AEnemy* Enemy = Cast<AEnemy>(Attacker)) Enemy->ReceiveParryStun(AttackerStunDuration);
    if (SuccessMontage)
    {
        // 이전 태스크의 중단 콜백을 해제한 뒤 성공 동작으로 교체한다.
        if (MontageTask)
        {
            MontageTask->EndTask();
            MontageTask = nullptr;
        }

        MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, SuccessMontage, 1.0f, NAME_None, true, 1.0f, 0.0f, true);
        MontageTask->OnCompleted.AddDynamic(this, &UGAParry::StartRecovery);
        MontageTask->OnInterrupted.AddDynamic(this, &UGAParry::HandleMontageInterrupted);
        MontageTask->OnCancelled.AddDynamic(this, &UGAParry::HandleMontageInterrupted);
        MontageTask->ReadyForActivation();
    }
    else if (!MontageTask)
    {
        StartRecovery();
    }

    if (!IsActive()) return;

    FGameplayCueParameters Cue;
    Cue.Instigator = Attacker;
    Cue.Location = GetAvatarActorFromActorInfo()->GetActorLocation();
    GetAbilitySystemComponentFromActorInfo()->ExecuteGameplayCue(EclipseGameplayTags::GameplayCue_Parry_Success, Cue);
    OnParrySucceeded(Attacker);
}

bool UGAParry::TryParry(AActor* Defender, AActor* Attacker, const FHitResult& Hit, bool bCanBeParried)
{
    if (!bCanBeParried) return false;
    if (!IsValid(Attacker)) return false;

    APlayerCharacter* Player = Cast<APlayerCharacter>(Defender);
    if (!IsValid(Player)) return false;

    const UStatAttributeSet* Stats = Player->GetStatAttribute();
    if (!Stats) return false;
    if (Stats->GetHealth() <= 0.0f) return false;

    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    if (!ASC) return false;
    if (!ASC->HasMatchingGameplayTag(EclipseGameplayTags::State_ParryWindow)) return false;
    if ((Attacker->GetActorLocation().X - Player->GetActorLocation().X) * Player->GetLogicalFacingSign() <= 0.0f) return false;

    // 판정 통과 후 동기 이벤트에서 창을 닫으므로 재진입 타격은 실패한다.
    FGameplayEventData Payload;
    Payload.EventTag = EclipseGameplayTags::Event_Parry_Success;
    Payload.Instigator = Attacker;
    Payload.Target = Defender;
    Payload.ContextHandle = ASC->MakeEffectContext();
    Payload.ContextHandle.AddHitResult(Hit);
    ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
    return true;
}

void UGAParry::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    CloseWindow();
    MontageTask = nullptr;
    WindowTask = nullptr;

    if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
    {
        Player->SetParryMovementLocked(false);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
