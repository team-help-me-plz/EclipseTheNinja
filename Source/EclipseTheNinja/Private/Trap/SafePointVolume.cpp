


#include "Trap/SafePointVolume.h"
#include "Components/BoxComponent.h"
#include "Interface/Interactor.h"
#include "Engine/TargetPoint.h"

// Sets default values
ASafePointVolume::ASafePointVolume()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	this->RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(this->RootScene);

	this->CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	this->CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
	this->CollisionBox->SetupAttachment(GetRootComponent());
}

// Called when the game starts or when spawned
void ASafePointVolume::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[BeginPlay::OnCollisionBeginOverlap] %s의 SafePoint가 nullptr입니다"),
		*this->GetName()
	);
}

void ASafePointVolume::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	this->CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &ASafePointVolume::OnCollisionBeginOverlap);
}

void ASafePointVolume::OnCollisionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!this->SafePoint)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[ASafePointVolume::OnCollisionBeginOverlap] %s의 SafePoint가 nullptr입니다"),
			*this->GetName()
		);
	}
	if (!OtherActor || !OtherActor->Implements<UInteractor>())
	{
		return ;
	}
	IInteractor::Execute_RegisterSafePoint(OtherActor, this->SafePoint);
}

void ASafePointVolume::OnCollisionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{

}

// Called every frame
void ASafePointVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

