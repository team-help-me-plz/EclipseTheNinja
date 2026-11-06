// Fill out your copyright notice in the Description page of Project Settings.

#include "PCG/EclipseInteriorVolume.h"

#include "PCGComponent.h"
#include "PCGGraph.h"

namespace EclipseInteriorVolume
{
    // 테마 그래프 기본 경로
    TSoftObjectPtr<UPCGGraphInterface> MakeGraphPtr(const TCHAR* AssetName)
    {
        const FString Path = FString::Printf(
            TEXT("/Game/PCG/Graphs/%s.%s"),
            AssetName,
            AssetName
        );

        return TSoftObjectPtr<UPCGGraphInterface>(FSoftObjectPath(Path));
    }
}

AEclipseInteriorVolume::AEclipseInteriorVolume(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    this->ThemeGraphs.Add(EEclipseInteriorTheme::Storehouse, EclipseInteriorVolume::MakeGraphPtr(TEXT("PCG_Interior_Storehouse")));
    this->ThemeGraphs.Add(EEclipseInteriorTheme::Corridor, EclipseInteriorVolume::MakeGraphPtr(TEXT("PCG_Interior_Corridor")));
    this->ThemeGraphs.Add(EEclipseInteriorTheme::Tatami, EclipseInteriorVolume::MakeGraphPtr(TEXT("PCG_Interior_Tatami")));
    this->ThemeGraphs.Add(EEclipseInteriorTheme::Underground, EclipseInteriorVolume::MakeGraphPtr(TEXT("PCG_Interior_Underground")));

    // 새로 놓을 때 들어가는 그래프는 기본 테마(창고)
    this->DefaultGraph = this->ThemeGraphs.FindRef(EEclipseInteriorTheme::Storehouse);
    this->Size = FVector(2000.0, 600.0, 400.0);
}

void AEclipseInteriorVolume::ApplyThemeGraph()
{
    if (!this->PCGComponent)
    {
        return;
    }

    const TSoftObjectPtr<UPCGGraphInterface>* GraphPtr = this->ThemeGraphs.Find(this->Theme);
    if (!GraphPtr)
    {
        return;
    }

    // 같은 그래프면 다시 넣지 않는다(불필요한 재생성 방지)
    UPCGGraphInterface* Graph = GraphPtr->LoadSynchronous();
    if (!Graph || this->PCGComponent->GetGraph() == Graph->GetGraph())
    {
        return;
    }

    this->PCGComponent->SetGraphLocal(Graph);
}


#if WITH_EDITOR
void AEclipseInteriorVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName MemberName = PropertyChangedEvent.MemberProperty
        ? PropertyChangedEvent.MemberProperty->GetFName()
        : NAME_None;

    if (MemberName == GET_MEMBER_NAME_CHECKED(AEclipseInteriorVolume, Theme))
    {
        this->ApplyThemeGraph();
        this->Regenerate();
    }
    else if (MemberName == GET_MEMBER_NAME_CHECKED(AEclipseInteriorVolume, Params))
    {
        // 노드가 액터 값을 직접 읽으므로 PCG가 변경을 모른다 → 직접 다시 생성
        this->Regenerate();
    }
}
#endif
