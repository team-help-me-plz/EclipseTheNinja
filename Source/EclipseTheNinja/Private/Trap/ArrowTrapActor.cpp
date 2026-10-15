


#include "Trap/ArrowTrapActor.h"
#include "Trap/TrapArrowActor.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "Checkpoint/CheckpointBoundComponent.h"

#include "Components/StaticMeshComponent.h"

// Sets default values
AArrowTrapActor::AArrowTrapActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	this->RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(this->RootScene);

	this->MeshAssembly = CreateDefaultSubobject<UChildActorComponent>(TEXT("MeshAssembly"));
	this->MeshAssembly->SetupAttachment(GetRootComponent());

	this->ArrowSpawnPos = CreateDefaultSubobject<USceneComponent>(TEXT("ArrowSpawnPos"));
	this->ArrowSpawnPos->SetupAttachment(GetRootComponent());

	this->CheckpointBound = CreateDefaultSubobject<UCheckpointBoundComponent>(TEXT("CheckpointBound"));
}

void AArrowTrapActor::ResetToInitial_Implementation()
{
	GetWorldTimerManager().ClearTimer(this->ActivateCooldownTimer);
	this->bIsActivateCooldown = false;
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
	const FVector	SpawnDir = this->ArrowSpawnPos->GetForwardVector();
	const FVector	SpawnPos = this->ArrowSpawnPos->GetComponentLocation();
	const FRotator	SpawnRot = SpawnDir.Rotation();

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

		// 체크포인트 리스폰 시 날아가던 화살 제거
		if (UCheckpointSubsystem* Subsystem = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
		{
			Subsystem->RegisterRuntimeSpawnedActor(SpawnedArrow);
		}
	}
}

void AArrowTrapActor::ResetActivateCooldown()
{
	this->bIsActivateCooldown = false;
}

