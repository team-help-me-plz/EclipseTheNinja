


#include "PuzzelObject/BreakablePlatform.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interface/Interactor.h"

// Sets default values
ABreakablePlatform::ABreakablePlatform()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	this->RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(this->RootScene);

	this->CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	this->CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
	this->CollisionBox->SetupAttachment(GetRootComponent());

	this->Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	this->Mesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	this->Mesh->SetupAttachment(GetRootComponent());
}

// Called when the game starts or when spawned
void ABreakablePlatform::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABreakablePlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ABreakablePlatform::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	this->CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &ABreakablePlatform::OnCollisionBeginOverlap);
	this->CollisionBox->OnComponentEndOverlap.AddDynamic(this, &ABreakablePlatform::OnCollisionEndOverlap);
}

void ABreakablePlatform::OnCollisionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor)
	{
		return ;
	}
	if (OtherActor == this)
	{
		return ;
	}
	this->OverlapCount++;
	if (!OtherActor->Implements<UInteractor>())
	{
		return ;
	}
	if (this->State != EBreakablePlatformState::Ready)
	{
		return ;
	}
	this->State = EBreakablePlatformState::Breaking;
	GetWorldTimerManager().SetTimer(
		this->PlatformBreakTimer,
		this,
		&ABreakablePlatform::BreakPlatform,
		this->BreakTime,
		false
	);
}

void ABreakablePlatform::OnCollisionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor == this)
	{
		return;
	}
	this->OverlapCount--;
}

void ABreakablePlatform::BreakPlatform()
{
	this->State = EBreakablePlatformState::Broken;
	this->Mesh->SetVisibility(false);
	this->Mesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	GetWorldTimerManager().SetTimer(
		this->PlatformRestoreTimer,
		this,
		&ABreakablePlatform::TryRestore,
		this->RestoreTime,
		false
	);
}

void ABreakablePlatform::TryRestore()
{
	if (this->OverlapCount)
	{
		GetWorldTimerManager().SetTimer(
			this->PlatformRestoreTryTimer,
			this,
			&ABreakablePlatform::TryRestore,
			this->RestoreTryTime,
			false
		);
		return ;
	}
	this->RestorePlatform();
}

void ABreakablePlatform::RestorePlatform()
{
	this->State = EBreakablePlatformState::Ready;
	this->Mesh->SetVisibility(true);
	this->Mesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
}

