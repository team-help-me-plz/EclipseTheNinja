#include "GAS/Ability/GAThrowKunai.h"
#include "Character/Enemy.h"
#include "Character/PlayerCharacter.h"
#include "Clone/CloneCharacter.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Effects/GEBasicAttackDamage.h"
#include "GAS/StatAttributeSet.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "ObjectPool/ObjectPoolSubsystem.h"
#include "Projectile/KunaiProjectile.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/World.h"
#include "NativeGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ThrowKunai, "Ability.Attack.Kunai");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_KunaiRelease, "Event.Attack.Kunai.Release");

UGAThrowKunai::UGAThrowKunai()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

    FGameplayTagContainer Tags;
    Tags.AddTag(TAG_ThrowKunai);
    SetAssetTags(Tags);

    // 근접 공격과 투척이 같은 몽타주 슬롯을 동시에 사용하지 않도록 막는다.
    ActivationOwnedTags.AddTag(UGABaseAttack::GetAttackingTag());
    ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());
    DamageEffectClass = UGEBasicAttackDamage::StaticClass();
}

FGameplayTag UGAThrowKunai::GetReleaseEventTag()
{
    return TAG_KunaiRelease;
}

bool UGAThrowKunai::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    if (!Character || !ASC || !ThrowMontage || !ProjectileClass || !DamageEffectClass
        || !Character->GetMesh() || !Character->GetMesh()->GetAnimInstance()
        || !ASC->GetSet<UStatAttributeSet>()
        || ASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f)
    {
        return false;
    }

    // 지상에서만 투척 가능
    if (!Character->GetCharacterMovement()->IsMovingOnGround())
    {
        return false;
    }

    // 플레이어는 카메라 이동 모드에서만 투척한다
    if (const APlayerCharacter* Player = Cast<APlayerCharacter>(Character))
    {
        if (!Player->CanThrowKunai())
        {
            return false;
        }
    }

    const AEnemy* Enemy = Cast<AEnemy>(Character);

    return !Enemy || !Enemy->IsDead();
}

void UGAThrowKunai::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!ThrowMontage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
        return;
    }

    bReleased = false;
    RequestedTarget = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
    bHasInitialPlayerCursor = false;

    if (const APlayerCharacter* Player = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo()))
    {
        // 좌클릭 경로에서 갱신한 커서를 몽타주 재생 전에 저장한다.
        // 준비 동작 중 커서/몸 방향이 바뀌어도 이번 투척의 조준은 바뀌지 않는다.
        InitialPlayerForward = Player->GetActorForwardVector();
        bHasInitialPlayerCursor = Player->HasCursorTarget();
        InitialPlayerCursorLocation = Player->GetCursorWorldLocation();
    }

    // 몽타주 첫 프레임의 Notify도 놓치지 않기 위해
    // 몽타주 재생 전에 Notify 이벤트 수신 테스크 준비
    UAbilityTask_WaitGameplayEvent* ReleaseTask =
        UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, GetReleaseEventTag(), nullptr, false, true);
    ReleaseTask->EventReceived.AddDynamic(this, &UGAThrowKunai::OnKunaiReleased);
    ReleaseTask->ReadyForActivation();

    // 타임아웃을 통해 잘못 연결된 루프 몽타주를 사용해도 State.Attacking 태그가 영원히 남는 버그 방지
    const float EffectiveRate = FMath::Max(0.01f, PlayRate * ThrowMontage->RateScale);
    UAbilityTask_WaitDelay* Timeout = UAbilityTask_WaitDelay::WaitDelay(
        this, ThrowMontage->GetPlayLength() / EffectiveRate + 1.0f); // 재생 속도도 고려된 전체 재생 시간에서 1초 이후에 발생하도록 함
    Timeout->OnFinish.AddDynamic(this, &UGAThrowKunai::OnMontageInterrupted);
    Timeout->ReadyForActivation();
    
    // 몽타주 재생 태스크 준비
    UAbilityTask_PlayMontageAndWait* MontageTask = 
        UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ThrowMontage, FMath::Max(0.01f, PlayRate));
    MontageTask->OnCompleted.AddDynamic(this, &UGAThrowKunai::OnMontageCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &UGAThrowKunai::OnMontageInterrupted);
    MontageTask->OnCancelled.AddDynamic(this, &UGAThrowKunai::OnMontageInterrupted);
    MontageTask->ReadyForActivation();
}

FVector UGAThrowKunai::GetThrowDirection_Implementation(ACharacter* Character, const FVector& SpawnLocation) const
{
    if (const AActor* Target = RequestedTarget.Get())
    {
        return (Target->GetActorLocation() - SpawnLocation).GetSafeNormal();
    }

    if (const AEnemy* Enemy = Cast<AEnemy>(Character))
    {
        if (const AActor* Target = Enemy->GetCombatTarget())
        {
            return (Target->GetActorLocation() - SpawnLocation).GetSafeNormal();
        }
    }

    if (Character->IsA<APlayerCharacter>())
    {
        return bHasInitialPlayerCursor ? (InitialPlayerCursorLocation - SpawnLocation).GetSafeNormal() : InitialPlayerForward;
    }

    return Character->GetActorForwardVector();
}

void UGAThrowKunai::OnKunaiReleased(FGameplayEventData Payload)
{
    ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

    if (!IsActive() || bReleased
        || !Character || !ASC || ASC->GetAnimatingAbility() != this
        || (Payload.Instigator && Payload.Instigator != Character))
    {
        return;
    }

    // 준비 동작 중 점프하거나 발판에서 떨어졌다면 공중에서 발사하지 않는다.
    if (!Character->GetCharacterMovement()->IsMovingOnGround()
        || ASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f)
    {
        OnMontageInterrupted();
        return;
    }

    bReleased = true; // Notify가 여러 개 있어도 한 번의 활성화에서는 한 발만 생성한다.
    const FTransform Origin = !ThrowSocketName.IsNone() && Character->GetMesh()->DoesSocketExist(ThrowSocketName) 
        ? Character->GetMesh()->GetSocketTransform(ThrowSocketName)
        : Character->GetActorTransform();
    FVector SpawnLocation = Origin.TransformPosition(SpawnOffset);
    SpawnLocation.Y = Character->GetActorLocation().Y;
    FHitResult LaunchHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(KunaiMuzzle), false, Character);

    // 손과 발사 위치 사이에 분신이 있어도 투척을 막지 않도록 무시 목록에 추가
    if (Character->IsA<APlayerCharacter>())
    {
        for (TActorIterator<ACloneCharacter> It(Character->GetWorld()); It; ++It)
        {
            QueryParams.AddIgnoredActor(*It);
        }
    }
    else if (Character->IsA<AEnemy>())
    {
        // 겹쳐 있거나 손 앞에 서 있는 아군도 비행 중과 동일하게 통과한다.
        for (TActorIterator<AEnemy> It(Character->GetWorld()); It; ++It)
        {
            QueryParams.AddIgnoredActor(*It);
        }
    }

    // 손/오프셋이 벽 너머에 있으면 벽을 건너뛰어 발사하지 않는다.
    // 몸과 손 사이에 캐릭터(Pawn)가 있으면 취소하지 않고, 아래에서 생성 직후 명중으로 처리한다.
    const bool bLaunchBlocked =
        ProjectileClass->GetDefaultObject<AKunaiProjectile>()->SweepLaunchPath(
            Character->GetWorld(),
            Character->GetActorLocation(),
            SpawnLocation,
            QueryParams,
            LaunchHit
        );
    if (bLaunchBlocked && !Cast<APawn>(LaunchHit.GetActor()))
    {
        OnMontageInterrupted();
        return;
    }

    FVector Direction = GetThrowDirection(Character, SpawnLocation);
    Direction.Y = 0.0f; // 프로젝트 이동 평면과 동일하게 표창도 XZ 평면으로 투척한다.
    if (Direction.ContainsNaN() || !Direction.Normalize())
    {
        Direction = Character->GetActorForwardVector();
    }

    // GetThrowDirection을 BP에서 재정의해도 최종 발사 단계에서 위/아래 각도를 제한한다.
    // X의 절댓값으로 경사를 계산하므로 왼쪽으로 던질 때도 동일한 제한이 적용된다.
    // 손 소켓이 커서를 지나쳐도 몸이 향하는 쪽으로 투척한다.
    const FVector Facing = Character->IsA<APlayerCharacter>() ? InitialPlayerForward : Character->GetActorForwardVector();
    const float HorizontalSign = Facing.X >= 0.0f ? 1.0f : -1.0f;
    const float AimAngle = FMath::RadiansToDegrees(FMath::Atan2(Direction.Z, FMath::Abs(Direction.X)));
    const float ClampedAngle = FMath::Clamp(
        AimAngle,
        -FMath::Clamp(MaxDownwardThrowAngle, 0.0f, 90.0f),
        FMath::Clamp(MaxUpwardThrowAngle, 0.0f, 90.0f)
    );
    const float AngleRadians = FMath::DegreesToRadians(ClampedAngle);
    Direction = FVector(HorizontalSign * FMath::Cos(AngleRadians), 0.0f, FMath::Sin(AngleRadians));
    
    const float Damage = ASC->GetNumericAttribute(UStatAttributeSet::GetAttackPowerAttribute()) * DamageMultiplier;
    if (!FMath::IsFinite(Damage) || Damage <= 0.0f)
    {
        OnMontageInterrupted();
        return;
    }

    FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
    Context.AddSourceObject(Character);
    Context.AddInstigator(Character, Character);
    FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), Context);
    if (!Spec.IsValid())
    {
        OnMontageInterrupted();
        return;
    }
    Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Damage")), Damage);

    const FTransform SpawnTransform(Direction.Rotation(), SpawnLocation);
    UWorld* World = Character->GetWorld();
    AKunaiProjectile* Projectile = nullptr;

    if (UObjectPoolSubsystem* PoolSubsystem = World->GetSubsystem<UObjectPoolSubsystem>())
    {
        Projectile = PoolSubsystem->SpawnActor<AKunaiProjectile>(ProjectileClass, SpawnTransform, Character, Character);
    }
    else
    {
        // 풀 서브시스템이 없을 경우 직접 스폰
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = Character;
        SpawnParams.Instigator = Character;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Projectile = World->SpawnActor<AKunaiProjectile>(ProjectileClass, SpawnTransform, SpawnParams);
    }
    
    if (!Projectile)
    {
        OnMontageInterrupted();
        return;
    }

    // 피해와 발사자 정보를 전달하고 비행을 시작
    Projectile->InitializeKunai(ASC, Spec, Direction, ProjectileSpeed);

    // 체크포인트 리스폰 시 날아가거나 박혀 있는 표창을 풀에 반환한다.
    if (UCheckpointSubsystem* CheckpointSubsystem = World->GetSubsystem<UCheckpointSubsystem>())
    {
        CheckpointSubsystem->RegisterRuntimeSpawnedActor(Projectile);
    }

    // 근접한 대상은 비행 sweep이 시작 겹침으로 놓칠 수 있으므로
    // 미리 찾아놓은 충돌을 직접 적용
    if (bLaunchBlocked && IsValid(Projectile))
    {
        Projectile->HandleLaunchHit(LaunchHit);
    }
}

void UGAThrowKunai::OnMontageCompleted()
{
    if (!IsActive())
    {
        return;
    }

    if (!bReleased)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UGAThrowKunai::OnMontageCompleted()] %s: ThrowMontage에 KunaiRelease 노티파이가 없습니다."),
               *GetName());
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGAThrowKunai::OnMontageInterrupted()
{
    if (IsActive())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
    }
}

void UGAThrowKunai::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    RequestedTarget.Reset();
    bHasInitialPlayerCursor = false;

    // Super가 몽타주/이벤트/타임아웃 태스크와 State.Attacking을 정리한다.
    // 이미 날아간 표창은 독립적으로 충돌하거나 수명이 끝날 때 제거된다.
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
