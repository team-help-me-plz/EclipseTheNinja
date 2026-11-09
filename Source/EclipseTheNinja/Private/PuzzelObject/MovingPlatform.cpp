// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzelObject/MovingPlatform.h"
#include "Checkpoint/CheckpointBoundComponent.h"
#include "Component/ActivateHighlightComponent.h"
#include "Save/GameSaveSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "GameFramework/Character.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AMovingPlatform::AMovingPlatform()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    this->RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    SetRootComponent(this->RootScene);

    this->MeshAssembly = CreateDefaultSubobject<UChildActorComponent>(TEXT("MeshAssembly"));
    this->MeshAssembly->SetMobility(EComponentMobility::Movable);
    this->MeshAssembly->SetupAttachment(GetRootComponent());

    this->CheckpointBound = CreateDefaultSubobject<UCheckpointBoundComponent>(TEXT("CheckpointBound"));
    this->ActivateHighlight = CreateDefaultSubobject<UActivateHighlightComponent>(TEXT("ActivateHighlight"));
}

void AMovingPlatform::BeginPlay()
{
    Super::BeginPlay();

    //this->ElapsedTime = 0.0;
    this->EndPointFloat = static_cast<float>(this->TargetPoint.Num()) - 1.0f;
    this->CycleLength = EndPointFloat * 2.0f;
    this->bIsMovementUse = this->bIsMovement;

    // 이어하기라면 저장된 켜짐 상태와 위치에서 시작한다. DeltaTime 0으로 위치만 맞춘다
    UGameSaveSubsystem* Save = UGameSaveSubsystem::Get(this);
    FSavedActorState State;

    if (Save && Save->TakeActorState(this, State))
    {
        this->bIsMovementUse = State.bIsOn;
        this->RouteProgress = State.Progress;
        this->PlatformMovement(0.f);
    }
}

void AMovingPlatform::PlatformMovement(float DeltaTime)
{
    if (this->TargetPoint.Num() < 2)
    {
        return;
    }
    if (!this->bIsMovementUse && !this->bIsResetable)
    {
        return;
    }
    if (this->TargetPointTravelTime == 0.f)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[AMovingPlatform::PlatformMovement] %s의 TargetPointTravelTime이 0입니다"),
            *this->GetName()
        );
        return;
    }
    // 막힌 이동을 적용하지 않고 기존 경로 위치에서 반대 방향으로 이어 간다.
    const float PreviousProgress = RouteProgress;
    const bool bPreviousForward = bIsMoveForward;
    const float	Step = DeltaTime / this->TargetPointTravelTime;
    if (this->bIsMovementUse)
    {
        float	Phase = this->bIsMoveForward
            ? this->RouteProgress
            : this->CycleLength - this->RouteProgress;
        Phase = FMath::Fmod(Phase + Step, CycleLength);
        this->bIsMoveForward = Phase < this->EndPointFloat;
        this->RouteProgress = this->bIsMoveForward
            ? Phase
            : this->CycleLength - Phase;
    }
    else if (this->bIsResetable)
    {
        this->RouteProgress = FMath::Max(this->RouteProgress - Step, 0.0);
        this->bIsMoveForward = true;
    }
    const int32		SegmentIndex = FMath::Min(FMath::FloorToInt(this->RouteProgress), this->TargetPoint.Num() - 2);
    const double	Alpha = this->RouteProgress - SegmentIndex;

    const FVector NewLocation = FMath::Lerp(
        this->TargetPoint[SegmentIndex]->GetActorLocation(),
        this->TargetPoint[SegmentIndex + 1]->GetActorLocation(),
        Alpha);
    if (IsCharacterBlockingMove(NewLocation - GetActorLocation()))
    {
        RouteProgress = PreviousProgress;
        bIsMoveForward = !bPreviousForward;
        return;
    }
    SetActorLocation(NewLocation);
}

bool AMovingPlatform::IsCharacterBlockingMove(const FVector& Delta) const
{
    if (FMath::Abs(Delta.Z) <= KINDA_SMALL_NUMBER) return false;

    UWorld* World = GetWorld();
    if (!World) return false;

    FComponentQueryParams Params(SCENE_QUERY_STAT(MovingPlatformCharacter), this);
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors, true, true);
    Params.AddIgnoredActors(AttachedActors);

    // 정상 탑승자는 발판과 함께 움직이므로 충돌 반전 대상에서 제외한다.
    for (TActorIterator<ACharacter> It(World); It; ++It)
    {
        const UPrimitiveComponent* Base = Cast<UPrimitiveComponent>(It->GetMovementBaseObject());
        if (!IsValid(Base)) continue;

        const AActor* BaseOwner = Base->GetOwner();
        if (!IsValid(BaseOwner)) continue;
        if (BaseOwner == this || BaseOwner->IsAttachedTo(this)) Params.AddIgnoredActor(*It);
    }

    // 루트는 SceneComponent이므로 외형 자식 액터의 실제 충돌 형상을 스윕한다.
    TInlineComponentArray<UPrimitiveComponent*> Components;
    GetComponents(Components, true);
    for (UPrimitiveComponent* Component : Components)
    {
        if (!IsValid(Component)) continue;
        if (!Component->IsQueryCollisionEnabled()) continue;

        TArray<FHitResult> Hits;
        World->ComponentSweepMulti(Hits, Component, Component->GetComponentLocation(),
            Component->GetComponentLocation() + Delta, Component->GetComponentQuat(), Params);
        for (const FHitResult& Hit : Hits)
        {
            if (!Hit.bBlockingHit) continue;
            if (!IsValid(Cast<ACharacter>(Hit.GetActor()))) continue;
            // 접촉한 캐릭터에게서 멀어지는 이동은 허용해 제자리 방향 반전을 막는다.
            if (FVector::DotProduct(Delta, Hit.ImpactNormal) >= 0.0f) continue;
            return true;
        }
    }
    return false;
}

void AMovingPlatform::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    this->PlatformMovement(DeltaTime);
}

void AMovingPlatform::ToggleActivate_Implementation()
{
    if (!this->bIsActivatable)
    {
        return;
    }
    this->bIsMovementUse = !this->bIsMovementUse;

    // 원위치로 돌아가지 않는 발판은 꺼진 순간의 위치에 멈추므로 진행도도 함께 저장한다
    if (UGameSaveSubsystem* Save = UGameSaveSubsystem::Get(this))
    {
        FSavedActorState State;
        State.bIsOn = this->bIsMovementUse;
        State.Progress = this->RouteProgress;
        Save->RecordActorState(this, State);
    }
}

void AMovingPlatform::ShowActivationIndicator_Implementation()
{
    this->ActivateHighlight->ShowHighlight();
}

void AMovingPlatform::HideActivationIndicator_Implementation()
{
    this->ActivateHighlight->HideHighlight();
}

void AMovingPlatform::ResetToInitial_Implementation()
{
    this->bIsMovementUse = this->bIsMovement;
    this->RouteProgress = 0.f;
    if (this->TargetPoint.Num())
    {
        this->SetActorLocation(this->TargetPoint[0]->GetActorLocation());
    }
}

void AMovingPlatform::OnAfterReset_Implementation()
{
}


