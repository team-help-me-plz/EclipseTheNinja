// Fill out your copyright notice in the Description page of Project Settings.

#include "PCG/EclipseGroundVolume.h"

#include "PCGGraph.h"

namespace EclipseGroundVolume
{
    // 에셋 이름만으로 소프트 메시 경로를 만든다
    TSoftObjectPtr<UStaticMesh> MeshPath(const TCHAR* Folder, const TCHAR* AssetName)
    {
        const FString Path = FString::Printf(
            TEXT("%s/%s.%s"),
            Folder,
            AssetName,
            AssetName
        );

        return TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Path));
    }

    void AddScatter(
        FEclipseGroundParams& Params,
        const TSoftObjectPtr<UStaticMesh>& Mesh,
        float Weight,
        float ScaleMin,
        float ScaleMax
    )
    {
        FEclipseGroundScatterEntry& Entry = Params.Scatter.AddDefaulted_GetRef();
        Entry.Mesh = Mesh;
        Entry.Weight = Weight;
        Entry.ScaleMin = ScaleMin;
        Entry.ScaleMax = ScaleMax;
    }
}

AEclipseGroundVolume::AEclipseGroundVolume(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    using namespace EclipseGroundVolume;

    this->DefaultGraph = TSoftObjectPtr<UPCGGraphInterface>(FSoftObjectPath(TEXT("/Game/PCG/Graphs/PCG_Ground.PCG_Ground")));
    this->Size = FVector(20000.0, 3200.0, 100.0);

    const TCHAR* JvkFolder = TEXT("/Game/_ExternalAssets/JapaneseVillageKit/Meshes");
    const TCHAR* BrushFolder = TEXT("/Game/_ExternalAssets/PN_IvyPack/Meshes/brushObjects");

    // 기본 타일: 16m 흙 바닥
    FEclipseGroundTileEntry& Tile = this->Params.Tiles.AddDefaulted_GetRef();
    Tile.Mesh = MeshPath(JvkFolder, TEXT("SM_JVK_Paving_Ground_Dirt_01_16x16"));

    // 기본 흩뿌림 후보(밀도 기본 0이라 켜야 나온다): 덤불 위주, 돌 약간. 약 30cm 메시라 크게 키운다
    struct FDefaultScatter
    {
        const TCHAR* Folder;
        const TCHAR* Asset;
        float Weight;
        float ScaleMin;
        float ScaleMax;
    };

    const FDefaultScatter Defaults[] = {
        { BrushFolder, TEXT("brush_01"), 1.0f, 3.0f, 6.0f },
        { BrushFolder, TEXT("brush_03"), 1.0f, 3.0f, 6.0f },
        { BrushFolder, TEXT("brush_05"), 1.0f, 3.0f, 6.0f },
        { BrushFolder, TEXT("smallBrush_01"), 1.0f, 3.0f, 5.0f },
        { BrushFolder, TEXT("rottenBrush_01"), 0.5f, 3.0f, 5.0f },
        { JvkFolder, TEXT("SM_JVK_Roof_B_Rock_01"), 0.4f, 1.0f, 2.0f },
        { JvkFolder, TEXT("SM_JVK_Roof_B_Rock_03"), 0.4f, 1.0f, 2.0f },
    };

    for (const FDefaultScatter& Default : Defaults)
    {
        AddScatter(
            this->Params,
            MeshPath(Default.Folder, Default.Asset),
            Default.Weight,
            Default.ScaleMin,
            Default.ScaleMax
        );
    }
}
