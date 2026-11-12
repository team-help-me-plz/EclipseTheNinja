// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/WorldSettings.h"
#include "EclipseWorldSettings.generated.h"

/**
 * 프로젝트 전용 WorldSettings.
 *  - 레벨마다 World Settings 패널에서 입장 시 표시할 맵 이름을 지정한다.
 *  - 입장 시퀀스 뒤에 띄우려면 그 TutorialSequenceTrigger의 bShowMapNameOnEnd를 켠다 (여기서는 따로 설정하지 않는다).
 *  - DefaultEngine.ini의 WorldSettingsClass로 등록되어 모든 레벨에 사용된다.
 */
UCLASS()
class ECLIPSETHENINJA_API AEclipseWorldSettings : public AWorldSettings
{
    GENERATED_BODY()

public:
    /** 맵 입장 시 화면에 띄울 이름. 비어 있으면 표시하지 않는다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Name")
    FText MapDisplayName;

    /** false면 이름이 있어도 입장 시 표시하지 않는다(메인메뉴, 테스트 맵 등). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Name")
    bool bShowMapNameOnEnter = true;

    /**
     * 현재 월드의 표시할 맵 이름을 가져온다.
     * @return 표시해야 하면 true (WorldSettings가 이 클래스이고, 표시가 켜져 있고, 이름이 비어 있지 않을 때)
     */
    UFUNCTION(BlueprintPure, Category = "Map Name", meta = (WorldContext = "WorldContextObject"))
    static bool GetMapDisplayName(const UObject* WorldContextObject, FText& OutMapName);
};
