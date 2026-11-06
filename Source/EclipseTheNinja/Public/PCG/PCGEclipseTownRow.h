// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGSettings.h"
#include "PCGEclipseTownRow.generated.h"

class UEclipseTownHouseSet;

/**
 * 스플라인을 따라 집 블럭과 골목을 나눈다(PCG_TownRow의 Subdivide 그래머를 대신함).
 * 출력은 Subdivide Spline과 같은 모양이다: 모듈마다 포인트 하나, 위치 = 모듈 양 끝을 이은 선의 가운데,
 * X축 = 진행 방향, 바운드 X = ±Size/2.
 *  - 집: Count=1, Mesh(SoftObjectPath), Size, Roof(0 기와/1 나무), Stories, BlockIndex
 *  - 골목: Count=0, Size(골목 폭)
 * 집 폭은 HouseSet 메시 바운드에서 계산하므로 메시를 바꿔도 그래프를 고칠 필요가 없다.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class ECLIPSETHENINJA_API UPCGEclipseTownRowSettings : public UPCGSettings
{
    GENERATED_BODY()

public:
#if WITH_EDITOR
    // 그래프에 추가될 때의 노드 이름
    virtual FName GetDefaultNodeName() const override { return FName(TEXT("EclipseTownRow")); }

    // 그래프 에디터에 보이는 노드 제목
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGEclipseTownRow", "NodeTitle", "Eclipse Town Row"); }

    // 노드 색·분류(샘플러)
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

    // 집 후보 목록(테마)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houses", meta = (PCG_Overridable))
    TSoftObjectPtr<UEclipseTownHouseSet> HouseSet;

    // 블럭 크기·골목 폭·지붕 계열·집 선택 시드(부모 UPCGSettings::Seed와 별개. 그래프의 Seed 파라미터를 연결한다)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (PCG_Overridable))
    int32 LayoutSeed = 0;

    // 스플라인 시작에서 첫 블럭까지 비우는 거리(cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (PCG_Overridable, ClampMin = "0"))
    float RowOffsetX = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (PCG_Overridable, ClampMin = "1"))
    int32 BlockHousesMin = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (PCG_Overridable, ClampMin = "1"))
    int32 BlockHousesMax = 6;

    // 블럭 안 이웃 집끼리 겹치는 폭(cm). 모듈 크기 = 집 폭 - 이 값
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (PCG_Overridable))
    float HouseOverlap = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (PCG_Overridable, ClampMin = "0"))
    float AlleyMin = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (PCG_Overridable, ClampMin = "0"))
    float AlleyMax = 700.0f;

    // 블럭이 2층 집 블럭일 확률(0~1)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mix", meta = (PCG_Overridable, ClampMin = "0", ClampMax = "1"))
    float TwoStoryRatio = 0.5f;

    // 블럭이 나무지붕 블럭일 확률(0~1). 나무 블럭 다음 블럭은 항상 기와
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mix", meta = (PCG_Overridable, ClampMin = "0", ClampMax = "1"))
    float WoodRoofBlockRatio = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mix", meta = (PCG_Overridable, ClampMin = "1"))
    int32 WoodBlockHousesMin = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mix", meta = (PCG_Overridable, ClampMin = "1"))
    int32 WoodBlockHousesMax = 3;

protected:
    // 입력: 줄 스플라인
    virtual TArray<FPCGPinProperties> InputPinProperties() const override;

    // 출력: 집·골목 모듈 포인트
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

    // 실행 요소 생성
    virtual FPCGElementPtr CreateElement() const override;
};

class FPCGEclipseTownRowElement : public IPCGElement
{
protected:
    // 스플라인을 블럭·골목으로 나눠 모듈 포인트를 쓴다
    virtual bool ExecuteInternal(FPCGContext* Context) const override;

    // 집 세트 에셋 내용이 바뀌어도 설정 값은 같으므로 캐시하지 않는다
    virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

    // 메시 에셋을 동기 로드해 바운드를 읽으므로 게임 스레드에서만 실행
    virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
};
