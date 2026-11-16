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
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Util/EclipseCollisionChannels.h"

ACheckpoint::ACheckpoint()
{
    // 가로등 밝기 연출 중에만 틱을 켠다
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

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
    // 평소에는 빛을 내지 않고, 등록 연출 동안만 밝아졌다 어두워진다
    LampLight->SetIntensity(0.0f);
    LampLight->SetAttenuationRadius(450.0f);
    LampLight->SetLightColor(FLinearColor(1.0f, 0.76f, 0.38f));
    LampLight->SetCastShadows(false);

    // 체크포인트 갱신 사운드(ActivateSound)는 BP에서 등록한다

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
    // 밝아졌다 어두워지는 연출은 다른 체크포인트로 갱신돼도 끊지 않는다. 빛과 발광은 연출이 관리한다
    bCheckpointActive = bActive;
}

void ACheckpoint::StartLampFlash()
{
    if (FlashDuration <= KINDA_SMALL_NUMBER || (FlashPeakIntensity <= 0.0f && FlashPeakEmissive <= 0.0f))
    {
        return;
    }

    // 한 사이클이 도는 동안에는 다시 시작하지 않는다
    if (bFlashing)
    {
        return;
    }

    // 등록 시 틱을 켜고, 한 사이클이 끝나면 StopLampFlash에서 끈다
    FlashElapsed = 0.0f;
    bFlashing = true;
    SetLampFlashWeight(0.0f);
    if (IsValid(LampLight))
    {
        LampLight->SetVisibility(true);
    }
    SetActorTickEnabled(true);
}

void ACheckpoint::StopLampFlash()
{
    if (!bFlashing)
    {
        return;
    }

    bFlashing = false;
    SetActorTickEnabled(false);
    SetLampFlashWeight(0.0f);
    if (IsValid(LampLight))
    {
        LampLight->SetVisibility(false);
    }
}

void ACheckpoint::SetLampFlashWeight(float Weight)
{
    if (IsValid(LampLight))
    {
        LampLight->SetIntensity(FlashPeakIntensity * Weight);
    }
    if (IsValid(LampMID))
    {
        LampMID->SetScalarParameterValue(EmissiveParameterName, FlashPeakEmissive * Weight);
    }
}

void ACheckpoint::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bFlashing)
    {
        SetActorTickEnabled(false);
        return;
    }

    FlashElapsed += DeltaSeconds;
    const float Alpha = FlashElapsed / FlashDuration;
    if (Alpha >= 1.0f)
    {
        StopLampFlash();
        return;
    }

    // 0 → 최대 → 0으로 부드럽게 올라갔다 내려온다
    SetLampFlashWeight(FMath::Sin(Alpha * PI));
}

void ACheckpoint::ApplyLampState()
{
    // 에디터 표시용. 게임 중에는 BeginPlay에서 만든 동적 머티리얼이 발광을 관리한다
    if (IsValid(LampMesh) && IsValid(LampOffMaterial))
    {
        LampMesh->SetMaterial(0, LampOffMaterial);
    }

    // 가로등 빛은 연출 중에만 보인다 (StartLampFlash/StopLampFlash)
    if (IsValid(LampLight))
    {
        LampLight->SetVisibility(bFlashing);
    }
}

void ACheckpoint::BeginPlay()
{
    Super::BeginPlay();

    ActivationTrigger->SetHiddenInGame(true);
    ActivationTrigger->SetVisibility(false);

    // 가로등 메시의 발광 세기를 연출로 바꾸기 위해 동적 머티리얼을 만든다
    if (IsValid(LampMesh))
    {
        UMaterialInterface* BaseMaterial = IsValid(LampOnMaterial) ? LampOnMaterial.Get() : LampMesh->GetMaterial(0);
        if (IsValid(BaseMaterial))
        {
            LampMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
            LampMesh->SetMaterial(0, LampMID);
        }
    }

    // BP나 인스턴스에 저장된 값과 관계없이 평소에는 빛 밝기·메시 발광 0, 틱 꺼짐
    SetLampFlashWeight(0.0f);
    SetActorTickEnabled(false);

    // 몬스터가 체크포인트에 막히지 않게 한다. Trigger 프로필에는 Monster 채널이 없어 기본값 Block이 적용되므로 런타임에 지정한다.
    this->ActivationTrigger->SetCollisionResponseToChannel(ECC_Monster, ECR_Ignore);
    // 벽 붙기 트레이스(WallAttach)도 같은 이유로 기본값 Block이 되므로 무시하게 한다.
    this->ActivationTrigger->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);

    this->ActivationTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACheckpoint::OnTriggerBeginOverlap);

    if (bWaitForSequence) return;

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
    TryActivateForPawn(Cast<APawn>(OtherActor));
}

void ACheckpoint::RegisterAfterSequence()
{
    if (!bWaitForSequence) return;
    if (bRegisteredAfterSequence) return;

    UCheckpointSubsystem* Subsystem = GetWorld()->GetSubsystem<UCheckpointSubsystem>();
    if (!Subsystem) return;

    bRegisteredAfterSequence = true;
    Subsystem->RegisterCheckpoint(this);

    // 이어하기로 이미 복원된 체크포인트가 있으면 진행 상태를 보존한다.
    if (Subsystem->GetCurrentCheckpoint() == ECheckpointId::None)
    {
        Subsystem->ActivateCheckpoint(CheckpointId);
    }
}

void ACheckpoint::TryActivateForPawn(const APawn* Pawn)
{
    if (!IsValid(Pawn)) return;
    if (!Pawn->IsPlayerControlled()) return;
    if (bWaitForSequence && !bRegisteredAfterSequence) return;
    // 밝기 연출 한 사이클 동안은 다시 닿아도 저장·사운드·연출을 모두 무시한다
    if (bFlashing) return;

    if (UCheckpointSubsystem* Subsystem = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
    {
        // 갱신되면 Subsystem이 bCheckpointActive를 켠다. 꺼져 있다 켜진 경우만 새 체크포인트로 갱신된 것이다
        const bool bWasActive = bCheckpointActive;

        // 새 체크포인트면 갱신과 저장을 함께 한다. 실제 갱신 여부는 Subsystem에서 로그로 출력
        Subsystem->ActivateCheckpoint(this->CheckpointId);

        if (!bCheckpointActive)
        {
            return;
        }

        if (bWasActive)
        {
            // 현재 체크포인트에 다시 닿았으면 체력·진행 상황만 다시 저장한다
            Subsystem->ResaveCurrentCheckpoint();
        }

        // 닿을 때마다 사운드를 내고 밝아졌다 어두워진다
        if (ActivateSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, ActivateSound, GetActorLocation());
        }
        StartLampFlash();
    }
}
