#include "GAS/Ability/GAParry.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Interface/ParryDeflectable.h"
#include "Character/Enemy.h"
#include "Character/PlayerCharacter.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Ability/GASwiftStrike.h"
#include "GAS/Ability/GAWallAttach.h"
#include "GAS/Ability/GAWallSlide.h"
#include "GAS/Ability/GAAssassinateBase.h"
#include "GAS/Animation/AnimNotifyState_ParryWindow.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
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

    APlayerCharacter* Player = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
    if (!IsValid(Player))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
        return;
    }

    Player->SetParryMovementLocked(true);

    // 판정 중 들어오는 공격은 플레이어 ASC의 GE 적용 질의가 TryParryIncomingEffect로 넘긴다.
    // 몽타주에 Parry Window 노티파이가 있으면 그 구간에서만, 없으면 지금부터 몽타주가 끝날 때까지 판정한다.
    bWindowByNotify = ParryMontage && HasParryWindowNotify(ParryMontage);
    if (!bWindowByNotify)
    {
        OpenWindow();
    }
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

void UGAParry::OpenWindow()
{
    if (bWindowOpen) return;

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC) return;

    // 태그를 실제로 붙인 뒤에만 열린 상태로 표시해 CloseWindow가 없는 태그를 빼지 않게 한다.
    bWindowOpen = true;
    ASC->AddLooseGameplayTag(EclipseGameplayTags::State_ParryWindow);
}

void UGAParry::OpenWindowFromNotify()
{
    // 이미 막았거나 후딜에 들어갔으면 같은 몽타주의 노티파이가 다시 시작돼도 판정을 열지 않는다.
    if (!IsActive() || !bWindowByNotify || bSucceeded || bRecoveryStarted) return;
    OpenWindow();
}

void UGAParry::CloseWindowFromNotify()
{
    if (!bWindowByNotify) return;
    CloseWindow();
}

bool UGAParry::HasParryWindowNotify(const UAnimMontage* Montage)
{
    auto HasNotify = [](const UAnimSequenceBase* Anim)
    {
        return Anim && Anim->Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Event)
        {
            return Event.NotifyStateClass && Event.NotifyStateClass->IsA<UAnimNotifyState_ParryWindow>();
        });
    };

    if (HasNotify(Montage)) return true;

    for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
    {
        for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
        {
            if (HasNotify(Segment.GetAnimReference())) return true;
        }
    }
    return false;
}

UGAParry* UGAParry::FindActiveParry(UAbilitySystemComponent* ASC)
{
    if (!ASC) return nullptr;

    for (const FGameplayAbilitySpec& AbilitySpec : ASC->GetActivatableAbilities())
    {
        if (!AbilitySpec.IsActive()) continue;
        if (UGAParry* Parry = Cast<UGAParry>(AbilitySpec.GetPrimaryInstance())) return Parry;
    }
    return nullptr;
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

void UGAParry::HandleSuccess(AActor* Attacker, AActor* DeflectedProjectile)
{
    const bool bDeflectedProjectile = DeflectedProjectile != nullptr;
    bSucceeded = true;
    CloseWindow();

    // 불꽃은 성공 동작과 같이 적의 공격을 직접 막았을 때만 낸다. 투사체를 튕겨낼 때는 내지 않는다.
    if (!bDeflectedProjectile)
    {
        SpawnSuccessSpark(Cast<AEnemy>(Attacker));
    }

    // 성공 동작을 재생하는 동안 예전 판정 타이머가 후딜을 시작하지 않게 한다.
    if (WindowTask)
    {
        WindowTask->EndTask();
        WindowTask = nullptr;
    }

    // 성공 동작은 적의 공격을 직접 막았을 때만 재생한다. 투사체는 패링 동작을 그대로 이어 간다.
    // 슬로우 모션은 성공 동작에 넣은 Slow Motion 노티파이가 낸다.
    if (SuccessMontage && !bDeflectedProjectile)
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

bool UGAParry::TryParryIncomingEffect(UAbilitySystemComponent* DefenderASC, const FGameplayEffectSpec& Spec)
{
    if (!DefenderASC) return false;
    if (!DefenderASC->HasMatchingGameplayTag(EclipseGameplayTags::State_ParryWindow)) return false;

    FGameplayTagContainer AssetTags;
    Spec.GetAllAssetTags(AssetTags);
    if (AssetTags.HasTag(EclipseGameplayTags::Attack_Unparryable)) return false;

    // 판정 창 상태는 실행 중인 패링 인스턴스가 갖고 있다.
    UGAParry* Parry = FindActiveParry(DefenderASC);
    if (!Parry) return false;

    const FGameplayEffectContextHandle& Context = Spec.GetContext();
    return Parry->ResolveIncomingAttack(Context.GetInstigator(), Context.GetEffectCauser());
}

bool UGAParry::ResolveIncomingAttack(AActor* Attacker, AActor* Causer)
{
    if (!IsActive() || !bWindowOpen || bSucceeded) return false;

    APlayerCharacter* Player = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
    if (!IsValid(Player)) return false;

    // 적이 아닌 원인(함정, 낙하물 등)의 피해는 패링하지 않는다.
    IParryDeflectable* Deflectable = Cast<IParryDeflectable>(Causer);
    AEnemy* Enemy = Cast<AEnemy>(Attacker);
    if (!Deflectable && !IsValid(Enemy)) return false;

    // 투사체는 날아온 위치, 근접 공격은 공격자 위치가 바라보는 쪽에 있어야 한다.
    const AActor* Source = Deflectable ? Causer : Enemy;
    if ((Source->GetActorLocation().X - Player->GetActorLocation().X) * Player->GetLogicalFacingSign() <= 0.0f) return false;

    if (Deflectable)
    {
        if (!Deflectable->Deflect(Player)) return false;
    }
    else
    {
        Enemy->ReceiveParry(AttackerStunDuration);
    }

    HandleSuccess(Attacker, Deflectable ? Causer : nullptr);
    return true;
}

void UGAParry::SpawnSuccessSpark(const AEnemy* Attacker) const
{
    const APlayerCharacter* Player = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
    if (!IsValid(Player)) return;

    // 적마다 지정한 불꽃(예: 보스)이 있으면 그것을, 없으면 기본 불꽃을 쓴다.
    UNiagaraSystem* EnemyEffect = IsValid(Attacker) ? Attacker->GetParrySparkEffect() : nullptr;
    UNiagaraSystem* Effect = EnemyEffect ? EnemyEffect : SuccessSparkEffect.Get();
    if (!Effect) return;

    // 칼을 든 오른손 조금 앞을 칼끼리 부딪힌 자리로 본다.
    const FVector Facing(Player->GetLogicalFacingSign(), 0.0f, 0.0f);
    const USkeletalMeshComponent* Mesh = Player->GetMesh();
    const FVector Hand = Mesh && Mesh->DoesSocketExist(TEXT("hand_r"))
        ? Mesh->GetSocketLocation(TEXT("hand_r"))
        : Player->GetActorLocation();
    const FVector Location = Hand + Facing * SuccessSparkHandOffset.X + FVector(0.0f, 0.0f, SuccessSparkHandOffset.Y);

    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        Player, Effect, Location, Facing.Rotation(), FVector(SuccessSparkScale));
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
