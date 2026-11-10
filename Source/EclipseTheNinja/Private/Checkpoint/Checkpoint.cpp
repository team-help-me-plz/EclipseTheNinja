// Fill out your copyright notice in the Description page of Project Settings.

#include "Checkpoint/Checkpoint.h"
#include "Checkpoint/CheckpointSubsystem.h"

#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Util/EclipseCollisionChannels.h"

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
    this->ActivationTrigger->SetHiddenInGame(true);
    this->ActivationTrigger->SetVisibility(false);
    this->ActivationTrigger->ShapeColor = FColor::Green;

    this->RespawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("RespawnPoint"));
    this->RespawnPoint->SetupAttachment(this->SceneRoot);
    this->RespawnPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));

    LampMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LampMesh"));
    LampMesh->SetupAttachment(SceneRoot);
    LampMesh->SetRelativeLocation(FVector(0.0f, 60.0f, 0.0f));
    // 감지 볼륨을 늘려도 가로등의 크기는 변하지 않게 한다.
    LampMesh->SetAbsolute(false, false, true);
    LampMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LampMesh->SetGenerateOverlapEvents(false);
    LampMesh->SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> LampAsset(
        TEXT("/Game/_ExternalAssets/JapaneseVillageKit/Meshes/SM_JVK_Lamp_Street_01.SM_JVK_Lamp_Street_01"));
    if (LampAsset.Succeeded())
    {
        LampMesh->SetStaticMesh(LampAsset.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> OffMaterialAsset(
        TEXT("/Game/Materials/Checkpoint/MI_CheckpointLamp_Off.MI_CheckpointLamp_Off"));
    if (OffMaterialAsset.Succeeded())
    {
        LampOffMaterial = OffMaterialAsset.Object;
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> OnMaterialAsset(
        TEXT("/Game/Materials/Checkpoint/MI_CheckpointLamp_On.MI_CheckpointLamp_On"));
    if (OnMaterialAsset.Succeeded())
    {
        LampOnMaterial = OnMaterialAsset.Object;
    }

    LampLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LampLight"));
    LampLight->SetupAttachment(LampMesh);
    LampLight->SetRelativeLocation(FVector(0.0f, 0.0f, 98.0f));
    LampLight->SetMobility(EComponentMobility::Movable);
    LampLight->SetIntensityUnits(ELightUnits::Lumens);
    LampLight->SetIntensity(600.0f);
    LampLight->SetAttenuationRadius(450.0f);
    LampLight->SetLightColor(FLinearColor(1.0f, 0.76f, 0.38f));
    LampLight->SetCastShadows(false);

    // 체크포인트 갱신 기본 사운드(에셋 시트). BP에서 바꿀 수 있다
    static ConstructorHelpers::FObjectFinder<USoundBase> ActivateSoundAsset(
        TEXT("/Game/Audio/spell_v15.spell_v15"));
    if (ActivateSoundAsset.Succeeded())
    {
        ActivateSound = ActivateSoundAsset.Object;
    }

    ApplyLampState();
}

void ACheckpoint::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // 기존 Blueprint와 배치 인스턴스에 저장된 볼륨 표시 설정도 숨긴다.
    ActivationTrigger->SetHiddenInGame(true);
    ActivationTrigger->SetVisibility(false);
    ApplyLampState();
}

void ACheckpoint::SetCheckpointActive(bool bActive)
{
    bCheckpointActive = bActive;
    ApplyLampState();
}

void ACheckpoint::ApplyLampState()
{
    UMaterialInterface* Material = LampOffMaterial;
    if (bCheckpointActive)
    {
        Material = LampOnMaterial;
    }

    if (IsValid(LampMesh) && IsValid(Material))
    {
        LampMesh->SetMaterial(0, Material);
    }

    if (IsValid(LampLight))
    {
        LampLight->SetVisibility(bCheckpointActive);
    }
}

void ACheckpoint::BeginPlay()
{
    Super::BeginPlay();

    ActivationTrigger->SetHiddenInGame(true);
    ActivationTrigger->SetVisibility(false);

    // 몬스터가 체크포인트에 막히지 않게 한다. Trigger 프로필에는 Monster 채널이 없어 기본값 Block이 적용되므로 런타임에 지정한다.
    this->ActivationTrigger->SetCollisionResponseToChannel(ECC_Monster, ECR_Ignore);
    // 벽 붙기 트레이스(WallAttach)도 같은 이유로 기본값 Block이 되므로 무시하게 한다.
    this->ActivationTrigger->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);

    this->ActivationTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACheckpoint::OnTriggerBeginOverlap);

    if (UCheckpointSubsystem* Subsystem = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
    {
        Subsystem->RegisterCheckpoint(this);

        if (this->bActivateOnBeginPlay)
        {
            Subsystem->ActivateCheckpoint(this->CheckpointId);
        }
    }
}

void ACheckpoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (UCheckpointSubsystem* Subsystem = World->GetSubsystem<UCheckpointSubsystem>())
        {
            Subsystem->UnregisterCheckpoint(this);
        }
    }

    Super::EndPlay(EndPlayReason);
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
        // 갱신되면 Subsystem이 이 체크포인트의 가로등을 켠다. 꺼져 있다 켜진 경우만 갱신된 것으로 보고 소리를 낸다
        const bool bWasActive = bCheckpointActive;

        // 실제 갱신 여부는 Subsystem에서 로그로 출력
        Subsystem->ActivateCheckpoint(this->CheckpointId);

        if (!bWasActive && bCheckpointActive && ActivateSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, ActivateSound, GetActorLocation());
        }
    }
}
