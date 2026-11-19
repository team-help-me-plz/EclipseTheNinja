// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/MapNameSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Level/EclipseWorldSettings.h"
#include "Level/TutorialSequenceTrigger.h"

bool UMapNameSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UMapNameSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    // 시퀀스 뒤에 맵 이름을 띄우는 입장 시퀀스 트리거가 있으면 그 트리거가 끝날 때 요청하므로 여기서는 띄우지 않는다
    // (월드 BeginPlay 시점에 로드되어 있는 트리거만 확인한다)
    for (TActorIterator<ATutorialSequenceTrigger> It(&InWorld); It; ++It)
    {
        if (It->ShowsMapNameOnEnd())
        {
            UE_LOG(LogTemp, Log, TEXT("[UMapNameSubsystem::OnWorldBeginPlay()] %s 시퀀스 종료까지 맵 이름 표시 대기"), *It->GetActorNameOrLabel());
            return;
        }
    }

    RequestShowMapName();
}

void UMapNameSubsystem::ShowMapName(const UObject* WorldContextObject)
{
    const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
    if (UMapNameSubsystem* Subsystem = World ? World->GetSubsystem<UMapNameSubsystem>() : nullptr)
    {
        Subsystem->RequestShowMapName();
    }
}

void UMapNameSubsystem::RequestShowMapName()
{
    if (this->bShown || this->bPending)
    {
        return;
    }

    FText MapName;
    if (!AEclipseWorldSettings::GetMapDisplayName(this, MapName))
    {
        return;
    }

    this->PendingMapName = MapName;
    this->bPending = true;

    if (this->OnShowMapName.IsBound())
    {
        Broadcast();
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[UMapNameSubsystem::RequestShowMapName()] 구독한 위젯이 없어 보류 : %s"), *MapName.ToString());
    }
}

void UMapNameSubsystem::FlushPendingMapName()
{
    if (this->bPending && this->OnShowMapName.IsBound())
    {
        Broadcast();
    }
}

void UMapNameSubsystem::Broadcast()
{
    this->bPending = false;
    this->bShown = true;

    UE_LOG(LogTemp, Log, TEXT("[UMapNameSubsystem::Broadcast()] 맵 이름 표시 : %s"), *this->PendingMapName.ToString());

    this->OnShowMapName.Broadcast(this->PendingMapName);
}
