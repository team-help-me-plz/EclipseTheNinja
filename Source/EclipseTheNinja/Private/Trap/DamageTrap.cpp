


#include "Trap/DamageTrap.h"
#include "Components/BoxComponent.h"
#include "Interface/Interactor.h"
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
    ACharacter* InCharacter = Cast<ACharacter>(OtherActor);
    if (!InCharacter)
    {
        return;
    }
    if (OtherComp != InCharacter->GetMesh())
    {
        return;
    }
    // 데미지 주기
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

