// Fill out your copyright notice in the Description page of Project Settings.

#include "PCG/EclipsePCGBoxVolume.h"

#include "EclipsePCGParams.h"
#include "PCGComponent.h"
#include "PCGGraph.h"

#if WITH_EDITOR
#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"
#include "Engine/Polys.h"
#include "Model.h"
#endif

namespace EclipsePCGBoxVolume
{
    const double MinSize = 10.0;

#if WITH_EDITOR
    // 브러시 폴리곤 꼭짓점으로 로컬 크기를 잰다. 브러시가 없으면 0
    FVector MeasureBrushSize(const AVolume* Volume)
    {
        const UModel* Brush = Volume->Brush;
        if (!Brush || !Brush->Polys)
        {
            return FVector::ZeroVector;
        }

        FBox Box(ForceInit);
        for (const FPoly& Poly : Brush->Polys->Element)
        {
            for (const FVector3f& Vertex : Poly.Vertices)
            {
                Box += FVector(Vertex);
            }
        }

        return Box.IsValid ? Box.GetSize() : FVector::ZeroVector;
    }
#endif
}

AEclipsePCGBoxVolume::AEclipsePCGBoxVolume(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void AEclipsePCGBoxVolume::SetBoxSize(const FVector& NewSize)
{
    this->Size = NewSize.ComponentMax(FVector(EclipsePCGBoxVolume::MinSize));
    this->RebuildBrushIfNeeded();
    this->ResetHandles();
    this->PushGraphParameters();

    if (this->PCGComponent)
    {
        this->PCGComponent->NotifyPropertiesChangedFromBlueprint();
    }
}

void AEclipsePCGBoxVolume::Regenerate()
{
    if (!this->PCGComponent)
    {
        return;
    }

    // 볼륨 크기가 바뀐 직후에도 새 바운드로 생성되도록 입력 데이터를 무효화하고 먼저 정리한다
    this->PCGComponent->NotifyPropertiesChangedFromBlueprint();
    this->PCGComponent->CleanupLocal(/*bRemoveComponents=*/true);
    this->PCGComponent->GenerateLocal(/*bForce=*/true);
}

void AEclipsePCGBoxVolume::ConvertScaleToSize()
{
#if WITH_EDITOR
    const FVector Scale = this->GetActorScale3D().GetAbs();
    if (Scale.Equals(FVector::OneVector, 1e-3))
    {
        return;
    }

    // 실제 브러시 크기 × 스케일이 지금 보이는 크기다
    FVector BrushSize = EclipsePCGBoxVolume::MeasureBrushSize(this);
    if (BrushSize.IsNearlyZero())
    {
        BrushSize = this->Size;
    }

    this->Modify();
    this->SetActorScale3D(FVector::OneVector);
    this->SetBoxSize(BrushSize * Scale);
#endif
}

void AEclipsePCGBoxVolume::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

#if WITH_EDITOR
    const UWorld* World = this->GetWorld();
    if (this->bRebuildingBrush || !World || World->IsGameWorld())
    {
        return;
    }

    this->EnsureDefaultGraph();
    this->ConvertScaleToSize();
    this->RebuildBrushIfNeeded();
    this->ResetHandles();
    this->PushGraphParameters();
#endif
}

void AEclipsePCGBoxVolume::PushGraphParameters()
{
    if (!this->PCGComponent)
    {
        return;
    }

    EclipsePCGParams::PushReflected(this, this->PCGComponent);
    this->PushComputedGraphParameters();
}

void AEclipsePCGBoxVolume::EnsureDefaultGraph()
{
    if (!this->PCGComponent || this->PCGComponent->GetGraph() || this->DefaultGraph.IsNull())
    {
        return;
    }

    UPCGGraphInterface* Graph = this->DefaultGraph.LoadSynchronous();
    if (Graph)
    {
        this->PCGComponent->SetGraphLocal(Graph);
    }
}

void AEclipsePCGBoxVolume::RebuildBrushIfNeeded()
{
#if WITH_EDITOR
    if (this->bRebuildingBrush || !GIsEditor)
    {
        return;
    }

    const FVector BrushSize = EclipsePCGBoxVolume::MeasureBrushSize(this);
    if (BrushSize.Equals(this->Size, 0.5))
    {
        return;
    }

    UCubeBuilder* Builder = NewObject<UCubeBuilder>(GetTransientPackage());
    Builder->X = static_cast<float>(this->Size.X);
    Builder->Y = static_cast<float>(this->Size.Y);
    Builder->Z = static_cast<float>(this->Size.Z);

    // 브러시 생성 함수가 PostEditChange를 부르므로 다시 들어오지 않게 막는다
    TGuardValue<bool> Guard(this->bRebuildingBrush, true);
    UActorFactory::CreateBrushForVolumeActor(this, Builder);
#endif
}

void AEclipsePCGBoxVolume::ResetHandles()
{
#if WITH_EDITORONLY_DATA
    const FVector Half = this->Size * 0.5;
    this->HandleXMax = FVector(Half.X, 0.0, 0.0);
    this->HandleXMin = FVector(-Half.X, 0.0, 0.0);
    this->HandleYMax = FVector(0.0, Half.Y, 0.0);
    this->HandleYMin = FVector(0.0, -Half.Y, 0.0);
    this->HandleZMax = FVector(0.0, 0.0, Half.Z);
#endif
}

#if WITH_EDITOR
bool AEclipsePCGBoxVolume::ApplyHandleDrag(FName PropertyName)
{
    const FVector Half = this->Size * 0.5;

    // 끌린 면의 바깥쪽 이동량(+면 늘어남)과 축
    int32 Axis = INDEX_NONE;
    double Sign = 1.0;
    double Grow = 0.0;

    if (PropertyName == GET_MEMBER_NAME_CHECKED(AEclipsePCGBoxVolume, HandleXMax))
    {
        Axis = 0;
        Grow = this->HandleXMax.X - Half.X;
    }
    else if (PropertyName == GET_MEMBER_NAME_CHECKED(AEclipsePCGBoxVolume, HandleXMin))
    {
        Axis = 0;
        Sign = -1.0;
        Grow = -this->HandleXMin.X - Half.X;
    }
    else if (PropertyName == GET_MEMBER_NAME_CHECKED(AEclipsePCGBoxVolume, HandleYMax))
    {
        Axis = 1;
        Grow = this->HandleYMax.Y - Half.Y;
    }
    else if (PropertyName == GET_MEMBER_NAME_CHECKED(AEclipsePCGBoxVolume, HandleYMin))
    {
        Axis = 1;
        Sign = -1.0;
        Grow = -this->HandleYMin.Y - Half.Y;
    }
    else if (PropertyName == GET_MEMBER_NAME_CHECKED(AEclipsePCGBoxVolume, HandleZMax))
    {
        Axis = 2;
        Grow = this->HandleZMax.Z - Half.Z;
    }

    if (Axis == INDEX_NONE)
    {
        return false;
    }

    // 최소 크기 아래로는 줄이지 않는다
    FVector NewSize = this->Size;
    NewSize[Axis] = FMath::Max(this->Size[Axis] + Grow, EclipsePCGBoxVolume::MinSize);
    const double Applied = NewSize[Axis] - this->Size[Axis];

    // 반대쪽 면이 제자리에 있도록 중심을 늘어난 양의 절반만큼 옮긴다
    FVector LocalShift = FVector::ZeroVector;
    LocalShift[Axis] = Sign * Applied * 0.5;

    this->Size = NewSize;
    this->SetActorLocation(this->GetActorLocation() + this->GetActorQuat().RotateVector(LocalShift));
    return true;
}

void AEclipsePCGBoxVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    // 파이썬 set_editor_property 등은 MemberProperty 없이 부르므로 속성 이름으로 대신한다
    const FName MemberName = PropertyChangedEvent.MemberProperty
        ? PropertyChangedEvent.MemberProperty->GetFName()
        : PropertyName;

    // 부모 처리(생성자 재실행)가 핸들을 되돌리기 전에 끌린 양을 먼저 반영한다
    const bool bHandleDragged = !this->bRebuildingBrush && this->ApplyHandleDrag(PropertyName);

    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (this->bRebuildingBrush)
    {
        return;
    }

    const bool bSizeChanged = bHandleDragged || MemberName == GET_MEMBER_NAME_CHECKED(AEclipsePCGBoxVolume, Size);
    if (bSizeChanged)
    {
        this->SetBoxSize(this->Size);
        return;
    }

    this->PushGraphParameters();

    // 노드가 액터 값을 직접 읽으면 PCG가 변경을 모른다 → 직접 다시 생성
    if (this->ReadsActorValuesDirectly() && this->PCGComponent)
    {
        this->PCGComponent->NotifyPropertiesChangedFromBlueprint();
    }
}

void AEclipsePCGBoxVolume::PostEditMove(bool bFinished)
{
    Super::PostEditMove(bFinished);

    if (this->bRebuildingBrush)
    {
        return;
    }

    // 스케일 기즈모로 늘렸으면 크기로 옮겨 담는다
    this->ConvertScaleToSize();
}
#endif
