#include "Character/BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/StatAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/StatAttributeSet.h"

ABaseCharacter::ABaseCharacter()
{
    ASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
    StatAttributeSet = CreateDefaultSubobject<UStatAttributeSet>(TEXT("Stat"));
    // 기본 공격은 배열의 기본 항목으로 둔다. BP에서 배열을 바꾸면 그 설정을 따른다.
    FAbilityInfo& BasicAttackInfo = AbilityClassArray.AddDefaulted_GetRef();
    BasicAttackInfo.AbilityClass = UGABaseAttack::StaticClass();
    BasicAttackInfo.InputID = EAbilityInputID::BasicAttack;
    bUseControllerRotationYaw = false;
    // 횡스크롤 이동을 XZ 평면으로 제한한다.
    GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, 1.0f, 0.0f));
    GetCharacterMovement()->bConstrainToPlane = true;


    GetMesh()->SetRelativeLocation(FVector(0, 0, -88.f));
    GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
}

UAbilitySystemComponent* ABaseCharacter::GetAbilitySystemComponent() const { return ASC; }
UStatAttributeSet* ABaseCharacter::GetStatAttribute() const { return StatAttributeSet; }

void ABaseCharacter::InitializeAbilitySystem()
{
    // 이 캐릭터를 GAS의 소유자와 실행 대상으로 연결한다.
    ASC->InitAbilityActorInfo(this, this);
    GiveInitAbility();
}

void ABaseCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 기존 BP의 이동 설정과 관계없이 시작 위치의 Y 평면을 유지한다.
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->SetPlaneConstraintNormal(FVector::YAxisVector);
    Movement->SetPlaneConstraintOrigin(GetActorLocation());
    Movement->SetPlaneConstraintEnabled(true);

    InitializeAbilitySystem();
    WarnIfAbilityMissing(EAbilityInputID::BasicAttack);
}

void ABaseCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitializeAbilitySystem();
}

void ABaseCharacter::PostLoad()
{
    Super::PostLoad();

    if (!bLegacyAbilitiesMigrated)
    {
        MigrateLegacyAbilities();
        bLegacyAbilitiesMigrated = true;
    }
}

void ABaseCharacter::MigrateLegacyAbilities()
{
    // 예전에는 BP 값이 없으면 C++ 기본 클래스로 부여했으므로 같은 규칙으로 옮긴다.
    MigrateLegacyAbility(EAbilityInputID::BasicAttack, BasicAttackAbilityClass_DEPRECATED
        ? TSubclassOf<UGameplayAbility>(BasicAttackAbilityClass_DEPRECATED)
        : TSubclassOf<UGameplayAbility>(UGABaseAttack::StaticClass()));
    BasicAttackAbilityClass_DEPRECATED = nullptr;
}

void ABaseCharacter::MigrateLegacyAbility(EAbilityInputID InputID, TSubclassOf<UGameplayAbility> AbilityClass)
{
    if (!AbilityClass) return;

    // 이미 같은 입력 ID가 지정돼 있으면 배열 설정을 우선한다.
    for (const FAbilityInfo& Info : AbilityClassArray)
    {
        if (Info.InputID == InputID) return;
    }

    FAbilityInfo& Info = AbilityClassArray.AddDefaulted_GetRef();
    Info.AbilityClass = AbilityClass;
    Info.InputID = InputID;
}

void ABaseCharacter::GiveInitAbility()
{
    // 권한이 있는 쪽에서 부여한다. 다시 호출돼도 중복 부여하지 않는다.
    if (!HasAuthority() || !ASC) return;

    for (const FAbilityInfo& Info : AbilityClassArray)
    {
        if (!Info.AbilityClass) continue;

        const int32 InputID = ToAbilityInputID(Info.InputID);
        const int32 Level = FMath::Max(1, Info.AbilityLevel);
        if (FGameplayAbilitySpec* Existing = ASC->FindAbilitySpecFromClass(Info.AbilityClass))
        {
            Existing->InputID = InputID;
            Existing->Level = Level;
            ASC->MarkAbilitySpecDirty(*Existing);
        }
        else
        {
            ASC->GiveAbility(FGameplayAbilitySpec(Info.AbilityClass, Level, InputID, this));
        }
    }
}

FGameplayAbilitySpec* ABaseCharacter::FindAbilitySpecByInputID(EAbilityInputID InputID) const
{
    if (!ASC || InputID == EAbilityInputID::None) return nullptr;
    return ASC->FindAbilitySpecFromInputID(ToAbilityInputID(InputID));
}

bool ABaseCharacter::TryActivateAbilityByInputID(EAbilityInputID InputID)
{
    const FGameplayAbilitySpec* Spec = FindAbilitySpecByInputID(InputID);
    return Spec && ASC->TryActivateAbility(Spec->Handle);
}

void ABaseCharacter::WarnIfAbilityMissing(EAbilityInputID InputID) const
{
    if (!HasAuthority() || FindAbilitySpecByInputID(InputID)) return;

    UE_LOG(LogTemp, Warning, TEXT("%s: AbilityClassArray에 %s 어빌리티가 없습니다."),
        *GetName(), *UEnum::GetValueAsString(InputID));
}

void ABaseCharacter::PlayHitReaction()
{
    // 이미 죽었거나, 몽타주가 없으면 종료
    if (bIsDead || !HitReactionMontage)
    {
        return;
    }

    // 공격 쿨타임에 몽타주 재생 시간이 추가되도록 조정
    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        // Restart on a new hit. Death is checked first and can replace this montage.
        MonategeDuration = AnimInstance->Montage_Play(HitReactionMontage, 1.0f, EMontagePlayReturnType::Duration);
    }
}

void ABaseCharacter::OnDeathMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
    if (bIsDead && Montage == DeathMontage)
    {
        // Hold the death pose instead of blending back to the locomotion graph.
        GetMesh()->bPauseAnims = true;
    }
}

void ABaseCharacter::DoBasicAttack()
{
    // 공격 실행은 GAS 어빌리티에 맡긴다.
    if (BasicAttackMontage)
    {
        // 이미 공격중인지 확인
        if (ASC->HasMatchingGameplayTag(UGABaseAttack::GetAttackingTag()))
        {
            // 공격 중이면 재실행 대신 입력 이벤트를 보낸다.
            FGameplayEventData Payload;
            Payload.EventTag = UGABaseAttack::GetComboInputEventTag();
            Payload.Instigator = this;

            // 콤보 입력용 이벤트 발생
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
                this, Payload.EventTag, Payload);

            return;
        }

        TryActivateAbilityByInputID(EAbilityInputID::BasicAttack);
    }
}

void ABaseCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    ASC->CancelAllAbilities();
    Super::EndPlay(EndPlayReason);
}
