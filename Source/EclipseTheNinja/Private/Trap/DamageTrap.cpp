


#include "Trap/DamageTrap.h"
#include "Interface/Interactor.h"
#include "Character/BaseCharacter.h"
#include "GAS/StatAttributeSet.h"
#include "GAS/Effects/GEBasicAttackDamage.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"

// Sets default values
ADamageTrap::ADamageTrap()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;

    this->RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    SetRootComponent(this->RootScene);

    this->CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    this->CollisionBox->SetMobility(EComponentMobility::Movable);
    this->CollisionBox->SetupAttachment(GetRootComponent());

}

// Called when the game starts or when spawned
void ADamageTrap::BeginPlay()
{
    Super::BeginPlay();

}

void ADamageTrap::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    this->CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &ADamageTrap::OnCollisionBeginOverlap);
    this->CollisionBox->OnComponentEndOverlap.AddDynamic(this, &ADamageTrap::OnCollisionEndOverlap);
}

void ADamageTrap::OnCollisionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || !OtherActor->Implements<UInteractor>())
    {
        return;
    }
    ABaseCharacter* InCharacter = Cast<ABaseCharacter>(OtherActor);
    if (!InCharacter)
    {
        return;
    }
    if (OtherComp != InCharacter->GetMesh())
    {
        return;
    }
    // 콜백 안에서 순간이동하면 같은 프레임에 남은 본별 오버랩이 다시 들어오므로 쿨다운으로 막는다
    const double CurrentTime = GetWorld()->GetTimeSeconds();
    if (const double* LastHitTime = this->LastHitTimes.Find(OtherActor))
    {
        if (CurrentTime - *LastHitTime < this->HitCooldown)
        {
            return;
        }
    }
    this->LastHitTimes.Add(OtherActor, CurrentTime);
    // 데미지 주기
	UAbilitySystemComponent* TargetASC = InCharacter->GetAbilitySystemComponent();
	// 플레이어면 데미지 주기
	if (TargetASC && TargetASC->GetSet<UStatAttributeSet>()
		&& TargetASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) > 0.f)
	{
		FGameplayEffectContextHandle	Context = TargetASC->MakeEffectContext();
		Context.AddInstigator(this, this);

		FGameplayEffectSpecHandle	Spec = TargetASC->MakeOutgoingSpec(UGEBasicAttackDamage::StaticClass(), 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Damage")), this->TrapDamage);
			TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
    IInteractor::Execute_ReturnToSafePoint(OtherActor);
}

void ADamageTrap::OnCollisionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{

}

// Called every frame
void ADamageTrap::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

}

