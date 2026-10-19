#include "GAS/Ability/GAThrowKunai.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy.h"
#include "Character/PlayerCharacter.h"
#include "Clone/CloneCharacter.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/World.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Effects/GEBasicAttackDamage.h"
#include "GAS/StatAttributeSet.h"
#include "NativeGameplayTags.h"
#include "Projectile/KunaiProjectile.h"
#include "GameFramework/CharacterMovementComponent.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ThrowKunai, "Ability.Attack.Kunai");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_KunaiRelease, "Event.Attack.Kunai.Release");

UGAThrowKunai::UGAThrowKunai()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    // 기존 기본 공격과 동일한 싱글플레이 로컬 실행 정책을 사용한다.
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
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
    const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!Character || !ASC || !ThrowMontage || !ProjectileClass || !DamageEffectClass
        || !Character->GetMesh() || !Character->GetMesh()->GetAnimInstance()
        || !ASC->GetSet<UStatAttributeSet>()
        || ASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f) return false;
    // 플레이어 입력/BP/적 AI 모두 지상에서만 투척을 시작한다.
    if (!Character->GetCharacterMovement()->IsMovingOnGround()) return false;
    const AEnemy* Enemy = Cast<AEnemy>(Character);
    // BP의 직접 활성화 경로에서도 플레이어는 카메라 이동 모드에서만 투척한다. 적 AI는 이 조건을 사용하지 않는다.
    if (const APlayerCharacter* Player = Cast<APlayerCharacter>(Character))
    {
        if (!Player->CanThrowKunai()) return false;
    }
    return !Enemy || !Enemy->IsDead();
}

void UGAThrowKunai::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

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

    if (!ThrowMontage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
        return;
    }

    // 몽타주 첫 프레임의 Notify도 놓치지 않기 위해
    // 몽타주 재생 전에 Notify 이벤트 수신 테스크 준비
    UAbilityTask_WaitGameplayEvent* ReleaseTask =
        UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, GetReleaseEventTag(), nullptr, false, true);
    ReleaseTask->EventReceived.AddDynamic(this, &UGAThrowKunai::OnKunaiReleased);
    ReleaseTask->ReadyForActivation();

    // 잘못 연결된 루프 몽타주도 State.Attacking을 영구히 남기지 않는다.
    UAbilityTask_WaitDelay* Timeout = UAbilityTask_WaitDelay::WaitDelay(
        this, ThrowMontage->GetPlayLength() / FMath::Max(0.01f, PlayRate) + 1.0f);
    Timeout->OnFinish.AddDynamic(this, &UGAThrowKunai::OnMontageInterrupted);
    Timeout->ReadyForActivation();

    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, ThrowMontage, FMath::Max(0.01f, PlayRate));
    MontageTask->OnCompleted.AddDynamic(this, &UGAThrowKunai::OnMontageCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &UGAThrowKunai::OnMontageInterrupted);
    MontageTask->OnCancelled.AddDynamic(this, &UGAThrowKunai::OnMontageInterrupted);
    MontageTask->ReadyForActivation();
}

FVector UGAThrowKunai::GetThrowDirection_Implementation(ACharacter* Character, const FVector& SpawnLocation) const
{
    if (const AActor* Target = RequestedTarget.Get()) return (Target->GetActorLocation() - SpawnLocation).GetSafeNormal();
    if (const AEnemy* Enemy = Cast<AEnemy>(Character))
    {
        if (const AActor* Target = Enemy->GetCombatTarget()) return (Target->GetActorLocation() - SpawnLocation).GetSafeNormal();
    }
    if (Character->IsA<APlayerCharacter>())
    {
        return bHasInitialPlayerCursor
            ? (InitialPlayerCursorLocation - SpawnLocation).GetSafeNormal() : InitialPlayerForward;
    }
    return Character->GetActorForwardVector();
}

void UGAThrowKunai::OnKunaiReleased(FGameplayEventData Payload)
{
    ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!IsActive() || bReleased || !Character || !ASC || ASC->GetAnimatingAbility() != this
        || (Payload.Instigator && Payload.Instigator != Character)) return;
    // 준비 동작 중 점프하거나 발판에서 떨어졌다면 공중에서 발사하지 않는다.
    if (!Character->GetCharacterMovement()->IsMovingOnGround()
        || ASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f)
    {
        OnMontageInterrupted();
        return;
    }

    // Notify가 여러 개 있어도 한 번의 활성화에서는 한 발만 생성한다.
    bReleased = true;
    const FTransform Origin = !ThrowSocketName.IsNone() && Character->GetMesh()->DoesSocketExist(ThrowSocketName)
        ? Character->GetMesh()->GetSocketTransform(ThrowSocketName) : Character->GetActorTransform();
    FVector SpawnLocation = Origin.TransformPosition(SpawnOffset);
    SpawnLocation.Y = Character->GetActorLocation().Y;
    // 손/오프셋이 벽 너머에 있어도 벽을 건너뛰어 발사하지 않는다.
    FHitResult Obstruction;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(KunaiMuzzle), false, Character);
    // 손과 발사 위치 사이에 아군 분신이 있어도 플레이어의 투척을 막지 않는다.
    if (Character->IsA<APlayerCharacter>())
    {
        for (TActorIterator<ACloneCharacter> It(Character->GetWorld()); It; ++It) QueryParams.AddIgnoredActor(*It);
    }
    const float Radius = ProjectileClass->GetDefaultObject<AKunaiProjectile>()->GetCollisionRadius();
    if (Character->GetWorld()->SweepSingleByChannel(Obstruction, Character->GetActorLocation(), SpawnLocation,
                                                    FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(FMath::Max(1.0f, Radius)), QueryParams))
    {
        OnMontageInterrupted();
        return;
    }
    FVector Direction = GetThrowDirection(Character, SpawnLocation);
    // 프로젝트 이동 평면과 동일하게 쿠나이도 XZ 평면으로 투척한다.
    Direction.Y = 0.0f;
    if (Direction.ContainsNaN() || !Direction.Normalize()) Direction = Character->GetActorForwardVector();

    // BP에서 방향을 재정의하더라도 최종 발사 단계에서 위/아래 각도를 제한한다.
    // X의 절댓값으로 경사를 계산하므로 왼쪽으로 던질 때도 동일한 제한이 적용된다.
    // 손 소켓이 커서를 지나쳐도 몸이 향하는 쪽으로 투척한다. 좌우는 소켓->커서 X값으로 정하지 않는다.
    const FVector Facing = Character->IsA<APlayerCharacter>() ? InitialPlayerForward : Character->GetActorForwardVector();
    const float HorizontalSign = Facing.X >= 0.0f ? 1.0f : -1.0f;
    const float AimAngle = FMath::RadiansToDegrees(FMath::Atan2(Direction.Z, FMath::Abs(Direction.X)));
    const float ClampedAngle = FMath::Clamp(AimAngle,
        -FMath::Clamp(MaxDownwardThrowAngle, 0.0f, 90.0f), FMath::Clamp(MaxUpwardThrowAngle, 0.0f, 90.0f));
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
    AKunaiProjectile* Projectile = Character->GetWorld()->SpawnActorDeferred<AKunaiProjectile>(
        ProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Projectile)
    {
        OnMontageInterrupted();
        return;
    }
    // BeginPlay/충돌보다 먼저 피해와 발사자 정보를 전달한다. GA 종료 후에도 발사체가 이를 보관한다.
    Projectile->InitializeKunai(ASC, Spec, Direction, ProjectileSpeed);
    Projectile->FinishSpawning(SpawnTransform);
}

void UGAThrowKunai::OnMontageCompleted()
{
    if (!IsActive()) return;
    if (!bReleased) UE_LOG(LogTemp, Warning, TEXT("%s: ThrowMontage needs a Kunai Release notify."), *GetName());
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGAThrowKunai::OnMontageInterrupted()
{
    if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

void UGAThrowKunai::EndAbility(const FGameplayAbilitySpecHandle Handle,
                               const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                               bool bReplicateEndAbility, bool bWasCancelled)
{
    RequestedTarget.Reset();
    bHasInitialPlayerCursor = false;
    // Super가 몽타주/이벤트/타임아웃 태스크와 State.Attacking을 정리한다.
    // 이미 날아간 쿠나이는 독립적으로 충돌하거나 수명이 끝날 때 제거된다.
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
