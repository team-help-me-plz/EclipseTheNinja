// Fill out your copyright notice in the Description page of Project Settings.

#include "Checkpoint/CheckpointRespawnVolume.h"
#include "Checkpoint/CheckpointSubsystem.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"

ACheckpointRespawnVolume::ACheckpointRespawnVolume()
{
    PrimaryActorTick.bCanEverTick = false;

    this->Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
    SetRootComponent(this->Trigger);

    this->Trigger->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));
    this->Trigger->SetCollisionProfileName(TEXT("Trigger"));
    this->Trigger->SetHiddenInGame(false);
    this->Trigger->ShapeColor = FColor::Red;
}

void ACheckpointRespawnVolume::BeginPlay()
{
    Super::BeginPlay();

    this->Trigger->OnComponentBeginOverlap.AddDynamic(this, &ACheckpointRespawnVolume::OnTriggerBeginOverlap);
}

void ACheckpointRespawnVolume::OnTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!IsValid(Pawn) || !Pawn->IsPlayerControlled())
    {
        return;
    }

    if (UCheckpointSubsystem* Subsystem = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
    {
        Subsystem->RespawnAtCheckpoint(Pawn);
    }
}
