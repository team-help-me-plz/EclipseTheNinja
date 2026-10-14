


#include "Trap/ArrowTrapActor.h"
#include "Trap/TrapArrowActor.h"

#include "Components/StaticMeshComponent.h"

// Sets default values
AArrowTrapActor::AArrowTrapActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	this->Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(this->Mesh);

	this->ArrowSpawnPos = CreateDefaultSubobject<USceneComponent>(TEXT("ArrowSpawnPos"));
	this->ArrowSpawnPos->SetupAttachment(GetRootComponent());
}

// Called when the game starts or when spawned
void AArrowTrapActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AArrowTrapActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AArrowTrapActor::ToggleActivate_Implementation()
{
	if (this->bIsActivateCooldown)
	{
		return ;
	}
	this->bIsActivateCooldown = true;
	GetWorldTimerManager().SetTimer(
		ActivateCooldownTimer,
		this,
		&AArrowTrapActor::ResetActivateCooldown,
		this->ActivateInterval,
		false
	);
	this->SpawnArrow();
}

void AArrowTrapActor::SpawnArrow()
{
	const FVector	SpawnDir = this->GetActorForwardVector();
	const FVector	SpawnPos = this->ArrowSpawnPos->GetComponentLocation();
	const FRotator	SpawnRot = this->GetActorRotation();

	FActorSpawnParameters	Params;
	Params.Owner = this;

	ATrapArrowActor* SpawnedArrow = GetWorld()->SpawnActor<ATrapArrowActor>(
		this->ArrowClass,
		SpawnPos,
		SpawnRot,
		Params
	);
	if (SpawnedArrow)
	{
		SpawnedArrow->SetLaunch(this->ArrowSpeed);
	}
}

void AArrowTrapActor::ResetActivateCooldown()
{
	this->bIsActivateCooldown = false;
}

