// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/ClimbLadder.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AClimbLadder::AClimbLadder()
{
    PrimaryActorTick.bCanEverTick = false;

    this->SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(this->SceneRoot);

    this->Rails = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rails"));
    this->Rails->SetupAttachment(this->SceneRoot);
    this->Rails->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    this->Rungs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rungs"));
    this->Rungs->SetupAttachment(this->SceneRoot);
    this->Rungs->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 벽타기 상자: 일반 벽처럼 모두 막고, 벽타기(WallClimb) 채널도 막아 GAWallAttach가 벽타기 벽으로 본다
    this->ClimbSurface = CreateDefaultSubobject<UBoxComponent>(TEXT("ClimbSurface"));
    this->ClimbSurface->SetupAttachment(this->SceneRoot);
    this->ClimbSurface->SetCollisionProfileName(TEXT("Custom"));
    this->ClimbSurface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    this->ClimbSurface->SetCollisionObjectType(ECC_WorldStatic);
    this->ClimbSurface->SetCollisionResponseToAllChannels(ECR_Block);
    this->ClimbSurface->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore); // Trap
    this->ClimbSurface->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Ignore); // Noise
    this->ClimbSurface->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Block);  // WallClimb
    this->ClimbSurface->ShapeColor = FColor::Green;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BambooMesh(TEXT("/Game/_ExternalAssets/JapaneseVillageKit/Meshes/SM_JVK_Bamboo_02.SM_JVK_Bamboo_02"));
    if (BambooMesh.Succeeded())
    {
        this->RailMesh = BambooMesh.Object;
        this->RungMesh = BambooMesh.Object;
    }
}

void AClimbLadder::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildLadder();
}

FTransform AClimbLadder::MakePoleTransform(const UStaticMesh* Mesh, const FVector& Start, const FRotator& Rotation, float Length, float Thickness) const
{
    const FBox Bounds = Mesh->GetBoundingBox();
    const FVector Size = Bounds.GetSize();
    const float ScaleZ = Length / FMath::Max(Size.Z, 1.0f);
    const float ScaleXY = Thickness / FMath::Max(FMath::Max(Size.X, Size.Y), 0.1f);

    // 메시 원점이 막대 아래 끝이 아닐 수 있으므로 아래 끝(Min.Z)이 Start에 오도록 옮긴다
    const FVector Scale(ScaleXY, ScaleXY, ScaleZ);
    const FVector MeshBottomOffset = Rotation.RotateVector(FVector(0.0f, 0.0f, Bounds.Min.Z) * Scale);
    return FTransform(Rotation, Start - MeshBottomOffset, Scale);
}

void AClimbLadder::RebuildLadder()
{
    this->Rails->ClearInstances();
    this->Rungs->ClearInstances();
    this->Rails->SetStaticMesh(this->RailMesh);
    this->Rungs->SetStaticMesh(this->RungMesh);

    // 액터 +X가 벽 안쪽이므로 사다리는 -X(벽 바깥)로 Width만큼 튀어나온다
    const float InnerX = -this->WallGap;
    const float OuterX = -FMath::Max(this->Width, this->WallGap + 1.0f);
    const float Y = this->VisualYOffset;

    if (IsValid(this->RailMesh))
    {
        const float RailLength = this->Height + this->RailTopExtension;
        for (const float RailX : {InnerX, OuterX})
        {
            this->Rails->AddInstance(MakePoleTransform(this->RailMesh, FVector(RailX, Y, 0.0f), FRotator::ZeroRotator, RailLength, this->RailThickness));
        }
    }

    if (IsValid(this->RungMesh))
    {
        // 메시 Z축을 -X로 눕혀 바깥 레일에서 안쪽 레일까지 잇는다 (Pitch 90: +Z → -X)
        const FRotator RungRotation(90.0f, 0.0f, 0.0f);
        const float RungLength = InnerX - OuterX;
        for (float Z = this->RungSpacing * 0.5f; Z <= this->Height; Z += this->RungSpacing)
        {
            this->Rungs->AddInstance(MakePoleTransform(this->RungMesh, FVector(InnerX, Y, Z), RungRotation, RungLength, this->RungThickness));
        }
    }

    // 벽타기 상자는 벽면(X=0)에서 벽 안쪽으로, 플레이 라인(Y=0) 앞뒤로 둔다
    this->ClimbSurface->SetCollisionEnabled(this->bProvidesClimbSurface ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    this->ClimbSurface->SetBoxExtent(FVector(this->ClimbSurfaceDepth * 0.5f, this->ClimbSurfaceHalfWidth, this->Height * 0.5f));
    this->ClimbSurface->SetRelativeLocation(FVector(this->ClimbSurfaceDepth * 0.5f, 0.0f, this->Height * 0.5f));
}
