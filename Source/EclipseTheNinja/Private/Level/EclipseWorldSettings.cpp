// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/EclipseWorldSettings.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

bool AEclipseWorldSettings::GetMapDisplayName(const UObject* WorldContextObject, FText& OutMapName)
{
    OutMapName = FText::GetEmpty();

    const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
    if (!World)
    {
        return false;
    }

    const AEclipseWorldSettings* Settings = Cast<AEclipseWorldSettings>(World->GetWorldSettings());
    if (!Settings || !Settings->bShowMapNameOnEnter || Settings->MapDisplayName.IsEmptyOrWhitespace())
    {
        return false;
    }

    OutMapName = Settings->MapDisplayName;
    return true;
}

bool AEclipseWorldSettings::ShouldReturnToPreviousStageOnDeath(const UObject* WorldContextObject)
{
    const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    const AEclipseWorldSettings* Settings = World ? Cast<AEclipseWorldSettings>(World->GetWorldSettings()) : nullptr;

    return Settings && Settings->bReturnToPreviousStageOnDeath;
}
