// Fill out your copyright notice in the Description page of Project Settings.

#include "PCG/EclipseMountainRange.h"

#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "UObject/ConstructorHelpers.h"

namespace EclipseMountainRange
{
    const int32 MaxLayers = 3;

    // 앞뒤 방향 격자 줄 수(앞 가장자리 → 능선 → 뒤 가장자리)
    const int32 DepthRows = 6;

    const FName ColorParam = TEXT("Color");

    // 0~1 능선 높이. 큰 굴곡 + 뾰족한 봉우리 + 잔 굴곡
    double RidgeHeight(double X, double PeakSpacing, double Roughness, const FVector2D& Offset)
    {
        const double Large = 0.5 + 0.5 * FMath::PerlinNoise1D(static_cast<float>(X / (PeakSpacing * 2.0) + Offset.X));

        // 1 - |노이즈|는 0 근처에서 뾰족한 봉우리가 된다
        const double Peaks = 1.0 - FMath::Abs(FMath::PerlinNoise1D(static_cast<float>(X / PeakSpacing + Offset.Y)));
        const double Detail = FMath::PerlinNoise1D(static_cast<float>(X / (PeakSpacing * 0.15) + Offset.X * 3.0));

        const double Height = 0.45 * Large + 0.55 * Peaks * Peaks + 0.15 * Roughness * Detail;
        return FMath::Clamp(Height, 0.0, 1.0);
    }

    // 앞뒤 단면 모양(0 = 앞 가장자리, 1 = 뒤 가장자리). 가운데가 능선
    double DepthProfile(double T)
    {
        // 끝(T=1)에서 Sin이 아주 작은 음수가 되면 Pow가 NaN이 되므로 0 아래를 자른다
        return FMath::Pow(FMath::Max(FMath::Sin(T * UE_PI), 0.0), 0.7);
    }
}

AEclipseMountainRange::AEclipseMountainRange()
{
    this->PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = this->CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    this->SetRootComponent(Root);

    // 겹마다 메시 컴포넌트 하나(배경 전용: 콜리전·그림자 없음)
    for (int32 Index = 0; Index < EclipseMountainRange::MaxLayers; ++Index)
    {
        const FName Name = *FString::Printf(TEXT("Layer%d"), Index);
        UDynamicMeshComponent* Component = this->CreateDefaultSubobject<UDynamicMeshComponent>(Name);
        Component->SetupAttachment(Root);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCastShadow(false);
        this->LayerComponents.Add(Component);
    }

    // 기본 세 겹: 앞은 낮고 어둡게, 뒤는 높고 밝게
    FEclipseMountainLayer Near;
    Near.Distance = 0.0f;
    Near.BaseHeight = 1500.0f;
    Near.PeakHeight = 6000.0f;
    Near.PeakSpacing = 14000.0f;
    Near.Color = FLinearColor(0.10f, 0.12f, 0.12f);

    FEclipseMountainLayer Mid;
    Mid.Distance = 15000.0f;
    Mid.BaseHeight = 3000.0f;
    Mid.PeakHeight = 10000.0f;
    Mid.PeakSpacing = 20000.0f;
    Mid.Color = FLinearColor(0.16f, 0.18f, 0.22f);

    FEclipseMountainLayer Far;
    Far.Distance = 32000.0f;
    Far.BaseHeight = 5000.0f;
    Far.PeakHeight = 15000.0f;
    Far.PeakSpacing = 28000.0f;
    Far.Roughness = 0.25f;
    Far.Color = FLinearColor(0.24f, 0.27f, 0.33f);

    this->RidgeLayers = { Near, Mid, Far };

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (BasicMaterial.Succeeded())
    {
        this->Material = BasicMaterial.Object;
    }
}

void AEclipseMountainRange::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // 게임 중에는 레벨에 저장된 메시를 그대로 쓴다
    const UWorld* World = this->GetWorld();
    if (World && World->IsGameWorld())
    {
        return;
    }

    this->Rebuild();
}

void AEclipseMountainRange::Rebuild()
{
    // 겹 수는 컴포넌트 수(3)를 넘지 않는다
    if (this->RidgeLayers.Num() > EclipseMountainRange::MaxLayers)
    {
        this->RidgeLayers.SetNum(EclipseMountainRange::MaxLayers);
    }

    for (int32 Index = 0; Index < this->LayerComponents.Num(); ++Index)
    {
        this->BuildLayer(Index);
    }

#if WITH_EDITORONLY_DATA
    this->EndHandle = FVector(this->Length, 0.0, 0.0);
#endif
}

void AEclipseMountainRange::BuildLayer(int32 LayerIndex)
{
    using namespace UE::Geometry;

    UDynamicMeshComponent* Component = this->LayerComponents[LayerIndex];
    if (!Component)
    {
        return;
    }

    const bool bUse = this->RidgeLayers.IsValidIndex(LayerIndex) && this->RidgeLayers[LayerIndex].bEnabled;
    Component->SetVisibility(bUse);
    if (!bUse)
    {
        Component->SetMesh(FDynamicMesh3());
        return;
    }

    const FEclipseMountainLayer& Layer = this->RidgeLayers[LayerIndex];

    // 겹마다 다른 노이즈 구간을 쓰도록 시드 + 겹 번호로 오프셋을 뽑는다
    FRandomStream Stream(this->Seed * 31 + LayerIndex * 7919);
    const FVector2D NoiseOffset(Stream.FRandRange(0.0, 1000.0), Stream.FRandRange(0.0, 1000.0));

    // 파이썬 등으로 범위 밖 값이 들어와도 격자가 폭주하지 않게 간격 하한·열 수 상한을 둔다
    const double SafeStep = FMath::Max(this->GridStep, 100.0);
    const int32 Columns = FMath::Clamp(FMath::CeilToInt32(this->Length / SafeStep) + 1, 2, 4096);
    const int32 Rows = EclipseMountainRange::DepthRows;
    const double StepX = this->Length / (Columns - 1);

    FDynamicMesh3 Mesh;
    for (int32 Column = 0; Column < Columns; ++Column)
    {
        const double X = StepX * Column;
        const double Ridge = EclipseMountainRange::RidgeHeight(
            X,
            Layer.PeakSpacing,
            Layer.Roughness,
            NoiseOffset
        );
        const double RidgeZ = FMath::Lerp(Layer.BaseHeight, Layer.PeakHeight, Ridge);

        for (int32 Row = 0; Row < Rows; ++Row)
        {
            const double T = static_cast<double>(Row) / (Rows - 1);
            const double Y = -Layer.Distance - T * this->Thickness;
            const double Z = RidgeZ * EclipseMountainRange::DepthProfile(T);
            Mesh.AppendVertex(FVector3d(X, Y, Z));
        }
    }

    // 윗면이 바깥(위·카메라 쪽)을 보도록 감는다: 법선 = (V1-V0) x (V2-V0)
    for (int32 Column = 0; Column + 1 < Columns; ++Column)
    {
        for (int32 Row = 0; Row + 1 < Rows; ++Row)
        {
            const int32 A = Column * Rows + Row;
            const int32 B = (Column + 1) * Rows + Row;
            const int32 C = Column * Rows + Row + 1;
            const int32 D = (Column + 1) * Rows + Row + 1;

            Mesh.AppendTriangle(A, C, B);
            Mesh.AppendTriangle(B, C, D);
        }
    }

    Mesh.EnableAttributes();
    FMeshNormals::InitializeOverlayToPerVertexNormals(Mesh.Attributes()->PrimaryNormals(), /*bUseMeshVertexNormalsIfAvailable=*/false);
    FMeshNormals::QuickRecomputeOverlayNormals(Mesh);

    Component->SetMesh(MoveTemp(Mesh));

    // 겹 색은 재질의 Color 파라미터로 넣는다
    if (this->Material)
    {
        UMaterialInstanceDynamic* Instance = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0));
        if (!Instance || Instance->Parent != this->Material)
        {
            Instance = UMaterialInstanceDynamic::Create(this->Material, Component);
            Component->SetMaterial(0, Instance);
        }

        Instance->SetVectorParameterValue(EclipseMountainRange::ColorParam, Layer.Color);
    }
}

#if WITH_EDITOR
void AEclipseMountainRange::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    // 부모 처리(생성자 재실행)가 핸들을 되돌리기 전에 끌린 길이를 먼저 반영한다
    if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(AEclipseMountainRange, EndHandle))
    {
        this->Length = FMath::Max(this->EndHandle.X, 1000.0);
    }

    Super::PostEditChangeProperty(PropertyChangedEvent);

    this->Rebuild();
}
#endif
