// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MapNameSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShowMapName, const FText&, MapName);

/**
 * 맵 입장 시 맵 이름 표시 시점을 관리하는 서브시스템.
 *  - 이름과 표시 여부는 AEclipseWorldSettings에서 레벨마다 지정한다
 *  - 일반 맵 : 월드 BeginPlay에서 바로 표시를 요청한다
 *  - 입장 시퀀스 맵 : bShowMapNameOnEnd가 켜진 TutorialSequenceTrigger가 있으면 바로 띄우지 않고, 그 트리거가 시퀀스 종료 후 요청한다
 *  - 레벨당 한 번만 띄운다 (체크포인트 리스폰 등으로 다시 요청돼도 무시)
 *  - 요청 시점에 OnShowMapName을 구독한 위젯이 없으면 보류했다가, HUD가 구독 후 FlushPendingMapName을 부르면 그때 알린다
 */
UCLASS()
class ECLIPSETHENINJA_API UMapNameSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** 맵 이름을 표시할 때 알린다. HUD가 구독해 위젯 애니메이션을 재생한다. */
    UPROPERTY(BlueprintAssignable, Category = "Map Name")
    FOnShowMapName OnShowMapName;

    /** WorldContextObject가 속한 월드의 서브시스템에 맵 이름 표시를 요청한다. */
    UFUNCTION(BlueprintCallable, Category = "Map Name", meta = (WorldContext = "WorldContextObject"))
    static void ShowMapName(const UObject* WorldContextObject);

    /** 맵 이름 표시를 요청한다. 이름이 없거나 이미 띄웠으면 무시한다. */
    UFUNCTION(BlueprintCallable, Category = "Map Name")
    void RequestShowMapName();

    /** OnShowMapName을 구독한 직후 호출한다. 구독 전에 들어온 요청이 있으면 지금 알린다. */
    UFUNCTION(BlueprintCallable, Category = "Map Name")
    void FlushPendingMapName();

    /** 이 레벨에서 이미 맵 이름을 띄웠는지 */
    UFUNCTION(BlueprintPure, Category = "Map Name")
    bool HasShownMapName() const { return this->bShown; }

protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
    void Broadcast();

    bool bShown = false;
    bool bPending = false;
    FText PendingMapName;
};
