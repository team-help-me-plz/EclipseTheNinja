#include "Character/BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/StatAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"

ABaseCharacter::ABaseCharacter()
{
    ASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
    StatAttributeSet = CreateDefaultSubobject<UStatAttributeSet>(TEXT("Stat"));
    BasicAttackAbilityClass = UGABaseAttack::StaticClass();
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
    GiveBasicAttackAbility();
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
}

void ABaseCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitializeAbilitySystem();
}

void ABaseCharacter::GiveBasicAttackAbility()
{
    // 권한이 있는 쪽에서 기본 공격을 한 번만 부여한다.
    if (HasAuthority() && BasicAttackAbilityClass && !ASC->FindAbilitySpecFromClass(BasicAttackAbilityClass))
    {
        ASC->GiveAbility(FGameplayAbilitySpec(BasicAttackAbilityClass, 1, INDEX_NONE, this));
    }
}

void ABaseCharacter::DoBasicAttack()
{
    // 공격 실행은 GAS 어빌리티에 맡긴다.
    if (BasicAttackAbilityClass && BasicAttackMontage)
    {
        ASC->TryActivateAbilityByClass(BasicAttackAbilityClass);
    }
}

void ABaseCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    ASC->CancelAllAbilities();
    Super::EndPlay(EndPlayReason);
}
