// Fill out your copyright notice in the Description page of Project Settings.

#include "Checkpoint/Checkpoint.h"
#include "Checkpoint/CheckpointSubsystem.h"

#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/Pawn.h"

ACheckpoint::ACheckpoint()
{
    PrimaryActorTick.bCanEverTick = false;

    this->SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(this->SceneRoot);

    this->ActivationTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ActivationTrigger"));
    this->ActivationTrigger->SetupAttachment(this->SceneRoot);
    this->ActivationTrigger->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
    this->ActivationTrigger->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
    this->ActivationTrigger->SetCollisionProfileName(TEXT("Trigger"));
    this->ActivationTrigger->SetHiddenInGame(false);
    this->ActivationTrigger->ShapeColor = FColor::Green;

    this->RespawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("RespawnPoint"));
    this->RespawnPoint->SetupAttachment(this->SceneRoot);
    this->RespawnPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
}

void ACheckpoint::BeginPlay()
{
    Super::BeginPlay();

    this->ActivationTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACheckpoint::OnTriggerBeginOverlap);

    if (this->bActivateOnBeginPlay)
    {
        if (UCheckpointSubsystem* Subsystem = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
        {
            Subsystem->ActivateCheckpoint(this);
        }
    }
}

FTransform ACheckpoint::GetRespawnTransform() const
{
    return this->RespawnPoint->GetComponentTransform();
}

void ACheckpoint::OnTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    const APawn* Pawn = Cast<APawn>(OtherActor);
    if (!IsValid(Pawn) || !Pawn->IsPlayerControlled())
    {
        return;
    }

    if (UCheckpointSubsystem* Subsystem = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
    {
        UE_LOG(LogTemp, Log, TEXT("[%d]번 체크포인트 활성화."), this->CheckpointOrder);
        Subsystem->ActivateCheckpoint(this);
    }
}
