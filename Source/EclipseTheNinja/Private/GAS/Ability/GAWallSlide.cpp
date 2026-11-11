


#include "GAS/Ability/GAWallSlide.h"
#include "AbilitySystemComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Ability/GAWallAttach.h"
#include "GAS/StatAttributeSet.h"
#include "GameFramework/Character.h"
#include "NativeGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallSlide, "Ability.Movement.WallSlide");

UGAWallSlide::UGAWallSlide()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;


    FGameplayTagContainer Tags;
    Tags.AddTag(TAG_WallSlide);
    SetAssetTags(Tags);

    // 매달린 상태에서만 슬라이딩을 시작한다. 시작하면 매달리기는 취소된다 (ActivateAbility 전에 끝난다).
    ActivationOwnedTags.AddTag(GetWallSlideAbilityTag());
    ActivationRequiredTags.AddTag(UGAWallAttach::GetWallAttachingTag());

    ActivationOwnedTags.AddTag(UGABaseAttack::GetAttackingTag());
    ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());
}

void UGAWallSlide::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
    Super::OnGiveAbility(ActorInfo, Spec);
    if (HasAnyFlags(RF_ClassDefaultObject)) return;

    // 시전할 때 쓰는 몽타주를 비동기로 불러 둔다. 다 불러오기 전에 시전하면 기존처럼 그 자리에서 불러온다.
    AssetPreloader.Request({ SlideMontage.ToSoftObjectPath() });
}

void UGAWallSlide::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
    AssetPreloader.Cancel();
    Super::OnRemoveAbility(ActorInfo, Spec);
}

FGameplayTag UGAWallSlide::GetWallSlideAbilityTag()
{
    return TAG_WallSlide;
}


bool UGAWallSlide::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
        return false;

    const ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!IsValid(Character)) return false;

    const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!IsValid(ASC)) return false;

    if (!Character->GetMesh() || !Character->GetMesh()->GetAnimInstance())
        return false;

    if (!ASC->GetSet<UStatAttributeSet>() || ASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f)
        return false;

    return true;
}

void UGAWallSlide::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // InstancedPerActor이므로 이전 실행의 완료 상태를 초기화한다.
    bSildeCompleted = false;
    bSlideMovementStarted = false;

    const ACharacter* Player = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!IsValid(Player))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
        return;
    }


    // 밑에 바닥이 있는지 검사하기 위한 Sweep
    FHitResult Hit;
    const FVector Start = ActorInfo->AvatarActor->GetActorLocation();
    const FVector Dir = FVector::DownVector;
    const FVector End = Start + Dir * SlideMaxHeight;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(WallSlide), false, Player);

    // 캐릭터보다 살짝 작은 캡슐을 아래로 스윕하여 충돌 검사
    const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
    const ECollisionChannel PawnChannel = Capsule->GetCollisionObjectType();
    float SweepRadius = FMath::Max(1.f, Capsule->GetScaledCapsuleRadius() - 2.f);
    FCollisionShape Shape = FCollisionShape::MakeCapsule(SweepRadius, Capsule->GetScaledCapsuleHalfHeight());


    // 스윕에 걸리는게 없으면 (땅이 없으면) 종료
    FCollisionResponseParams ResponseParams;
    if (!GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, PawnChannel, Shape, Params, ResponseParams))
    {
        OnSlideInterrupted();
        return;
    }

    // 시작부터 관통한 상태면 종료
    if (Hit.bStartPenetrating)
    {
        OnSlideInterrupted();
        return;
    }

    FVector LandLocation = Hit.Location;

    // 밑에 Pawn(적, 분신이 있는 경우) 종료
    FCollisionObjectQueryParams CharacterObjects;
    CharacterObjects.AddObjectTypesToQuery(ECC_Pawn);
    SweepRadius = FMath::Max(1.f, Capsule->GetScaledCapsuleRadius() + 2.f);
    Shape = FCollisionShape::MakeCapsule(SweepRadius, Capsule->GetScaledCapsuleHalfHeight());

    if (GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, CharacterObjects, Shape, Params))
    {
        OnSlideInterrupted();
        return;
    }


    float SlideDist = (Player->GetActorLocation().Z - LandLocation.Z);

    if (SlideDist <= 0)
        SlideDist = 0;


    // 미끄러질 시간 계산 (벽 높이에 상관없이 동일한 속도로 떨어지기 때문에 Duration이 달라야함)
    const float Duration = SlideDist / SlideSpeed;
    if (Duration <= KINDA_SMALL_NUMBER)
    {
        OnSlideInterrupted();
        return;
    }

    // 매달리기 종료가 Falling으로 바꾸므로 먼저 취소한 뒤 슬라이드 이동 모드를 적용한다.
    FGameplayTagContainer CancelTags;
    CancelTags.AddTag(UGAWallAttach::GetWallAttachAbilityTag());
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    ASC->CancelAbilities(&CancelTags, nullptr, this);
    bSlideMovementStarted = true;

    // 아래로 미끄러지는 태스크 시작
    UAbilityTask_ApplyRootMotionMoveToForce* MoveTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
        this,
        NAME_None,
        LandLocation,
        Duration,
        true,
        MOVE_Flying,
        false,
        nullptr,
        ERootMotionFinishVelocityMode::SetVelocity,
        FVector::ZeroVector,
        0.f
    );

    MoveTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UGAWallSlide::OnSlideCompleted);
    MoveTask->OnTimedOut.AddDynamic(this, &UGAWallSlide::OnSlideInterrupted);
    MoveTask->ReadyForActivation();
    if (IsActive() && Duration > 0.0f && SlideLoopSound && Player->GetNetMode() != NM_DedicatedServer)
    {
        SlideAudioComponent = UGameplayStatics::SpawnSoundAttached(SlideLoopSound, Player->GetRootComponent(),
                                                                   NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true);
    }

    // 내려가는 동안 벽이 끝나면 루트 모션을 끝내고 중력으로 낙하한다.
    CheckSlideWall();
    if (!IsActive()) return;
    GetWorld()->GetTimerManager().SetTimer(WallCheckTimer, this, &UGAWallSlide::CheckSlideWall, 0.016f, true);

    // 몽타주 재생
    LodaedSlideMontage = SlideMontage.LoadSynchronous();
    if (IsValid(LodaedSlideMontage))
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, LodaedSlideMontage, 1.0f, NAME_None, true);
        MontageTask->ReadyForActivation();

        // 위치는 코드가 맡으므로 몽타주 루트 모션은 끈다
        UAnimInstance* AnimInstance = Player->GetMesh() ? Player->GetMesh()->GetAnimInstance() : nullptr;
        if (FAnimMontageInstance* MontageInstance = AnimInstance ? AnimInstance->GetActiveInstanceForMontage(LodaedSlideMontage) : nullptr)
        {
            MontageInstance->PushDisableRootMotion();
        }
    }
}

void UGAWallSlide::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (IsValid(SlideAudioComponent))
    {
        SlideAudioComponent->Stop();
    }
    SlideAudioComponent = nullptr;

    GetWorld()->GetTimerManager().ClearTimer(WallCheckTimer);
    ACharacter* Player = nullptr;
    if (ActorInfo)
    {
        Player = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    }

    // 이번 실행의 결과만 사용하고 종료 전에 상태를 비운다.
    bool bShouldFall = bSildeCompleted;
    if (bSlideMovementStarted) bShouldFall = true;
    bSildeCompleted = false;
    bSlideMovementStarted = false;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

    if (!bShouldFall) return;
    if (!IsValid(Player)) return;

    UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
    if (!IsValid(Movement)) return;

    Movement->StopMovementImmediately();
    Movement->SetMovementMode(MOVE_Falling);
}

void UGAWallSlide::CheckSlideWall()
{
    if (!IsActive()) return;

    const ACharacter* Player = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!IsValid(Player)) { OnSlideInterrupted(); return; }

    const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
    if (!IsValid(Capsule)) { OnSlideInterrupted(); return; }

    // 몸통 높이에 실제 매달릴 벽이 있는지 확인한다. 바닥이나 완만한 경사는 제외한다.
    const FVector Start = Player->GetActorLocation();
    const FVector End = Start + Player->GetActorForwardVector() * (Capsule->GetScaledCapsuleRadius() + 60.0f);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(WallSlideWall), false, Player);
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_GameTraceChannel2, Params))
    {
        OnSlideInterrupted();
        return;
    }
    if (FMath::Abs(Hit.ImpactNormal.Z) > 0.3f) OnSlideInterrupted();
}

void UGAWallSlide::OnSlideCompleted()
{
    if (!IsActive()) return;
    bSildeCompleted = true;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGAWallSlide::OnSlideInterrupted()
{
    if (!IsActive()) return;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}
