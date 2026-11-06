// Fill out your copyright notice in the Description page of Project Settings.

#include "PCG/EclipsePCGLineActor.h"

#include "EclipsePCGParams.h"
#include "Components/SplineComponent.h"
#include "PCGComponent.h"
#include "PCGGraph.h"

AEclipsePCGLineActor::AEclipsePCGLineActor()
{
    this->PrimaryActorTick.bCanEverTick = false;

    this->Spline = this->CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
    this->SetRootComponent(this->Spline);

    this->PCGComponent = this->CreateDefaultSubobject<UPCGComponent>(TEXT("PCG"));
}

void AEclipsePCGLineActor::SetLength(double NewLength)
{
    this->Length = FMath::Max(NewLength, 100.0);
    this->RefreshLine();
}

void AEclipsePCGLineActor::Regenerate()
{
    if (!this->PCGComponent)
    {
        return;
    }

    this->PCGComponent->NotifyPropertiesChangedFromBlueprint();
    this->PCGComponent->CleanupLocal(/*bRemoveComponents=*/true);
    this->PCGComponent->GenerateLocal(/*bForce=*/true);
}

void AEclipsePCGLineActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

#if WITH_EDITOR
    const UWorld* World = this->GetWorld();
    if (!World || World->IsGameWorld())
    {
        return;
    }

    this->RefreshLine();
#endif
}

void AEclipsePCGLineActor::BuildLinePoints(TArray<FVector>& OutPoints) const
{
    OutPoints.Add(FVector::ZeroVector);
    OutPoints.Add(FVector(this->Length, 0.0, 0.0));
}

void AEclipsePCGLineActor::RefreshLine()
{
    if (!this->Spline || !this->PCGComponent)
    {
        return;
    }

    // 그래프가 비어 있으면 기본 그래프를 넣는다
    if (!this->PCGComponent->GetGraph() && !this->DefaultGraph.IsNull())
    {
        if (UPCGGraphInterface* Graph = this->DefaultGraph.LoadSynchronous())
        {
            this->PCGComponent->SetGraphLocal(Graph);
        }
    }

    TArray<FVector> Points;
    this->BuildLinePoints(Points);

    // 점이 같으면 스플라인을 건드리지 않는다(불필요한 재생성 방지)
    bool bSame = this->Spline->GetNumberOfSplinePoints() == Points.Num();
    for (int32 Index = 0; bSame && Index < Points.Num(); ++Index)
    {
        const FVector Existing = this->Spline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::Local);
        bSame = Existing.Equals(Points[Index], 0.1);
    }

    if (!bSame)
    {
        this->Spline->ClearSplinePoints(/*bUpdateSpline=*/false);
        for (const FVector& Point : Points)
        {
            this->Spline->AddSplinePoint(Point, ESplineCoordinateSpace::Local, /*bUpdateSpline=*/false);
        }

        for (int32 Index = 0; Index < Points.Num(); ++Index)
        {
            this->Spline->SetSplinePointType(Index, ESplinePointType::Linear, /*bUpdateSpline=*/false);
        }

        this->Spline->SetClosedLoop(false, /*bUpdateSpline=*/false);
        this->Spline->UpdateSpline();
    }

#if WITH_EDITORONLY_DATA
    this->EndHandle = FVector(this->Length, 0.0, 0.0);
#endif

    EclipsePCGParams::PushReflected(this, this->PCGComponent);
    this->PushComputedGraphParameters();

    if (!bSame)
    {
        this->PCGComponent->NotifyPropertiesChangedFromBlueprint();
    }
}

#if WITH_EDITOR
void AEclipsePCGLineActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    // 부모 처리(생성자 재실행)가 핸들을 되돌리기 전에 끌린 길이를 먼저 반영한다
    if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(AEclipsePCGLineActor, EndHandle))
    {
        this->Length = FMath::Max(this->EndHandle.X, 100.0);
    }

    Super::PostEditChangeProperty(PropertyChangedEvent);

    this->RefreshLine();
}
#endif
