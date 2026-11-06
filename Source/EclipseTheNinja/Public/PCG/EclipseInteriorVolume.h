// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCG/EclipsePCGBoxVolume.h"
#include "PCG/EclipseInteriorTypes.h"
#include "EclipseInteriorVolume.generated.h"

class UPCGGraphInterface;

/**
 * 성 내부 방 하나를 만드는 PCG 볼륨 (볼륨 하나 = 방 하나).
 *  - 볼륨 X = 방 길이, -Y 면 = 뒷벽, +Y 면 = 바닥 앞 가장자리, 아래/윗면 = 마루/천장
 *  - 크기는 Size(cm)나 뷰포트 면 핸들로 바꾼다(스케일은 쓰지 않는다)
 *  - Theme을 고르면 테마 그래프(PCG_Interior_*)가 PCG 컴포넌트에 들어간다
 *  - Params는 그래프의 Eclipse Interior Bays 노드가 생성 때 읽는다
 * 생성물은 배경 전용(콜리전 없음). 회전 없이(yaw 0) 놓는 것을 전제로 한다.
 */
UCLASS(BlueprintType, DisplayName = "Eclipse Interior Volume")
class ECLIPSETHENINJA_API AEclipseInteriorVolume : public AEclipsePCGBoxVolume
{
    GENERATED_BODY()

public:
    AEclipseInteriorVolume(const FObjectInitializer& ObjectInitializer);

    // 노드가 읽는 방 파라미터
    const FEclipseInteriorRoomParams& GetRoomParams() const { return this->Params; }

    // 현재 Theme에 맞는 그래프를 PCG 컴포넌트에 넣는다
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Interior")
    void ApplyThemeGraph();

#if WITH_EDITOR
    // Theme이 바뀌면 그래프 교체, Params가 바뀌면 다시 생성
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
    // 방 테마. 바꾸면 그래프가 교체된다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interior")
    EEclipseInteriorTheme Theme = EEclipseInteriorTheme::Storehouse;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interior", meta = (ShowOnlyInnerProperties))
    FEclipseInteriorRoomParams Params;

    // 테마별 그래프. 기본값은 /Game/PCG/Graphs/PCG_Interior_*
    UPROPERTY(EditDefaultsOnly, Category = "Interior")
    TMap<EEclipseInteriorTheme, TSoftObjectPtr<UPCGGraphInterface>> ThemeGraphs;
};
