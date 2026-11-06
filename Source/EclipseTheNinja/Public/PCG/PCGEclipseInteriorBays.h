// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGSettings.h"
#include "PCG/EclipseInteriorTypes.h"
#include "PCGEclipseInteriorBays.generated.h"

/**
 * 성 내부 방 볼륨을 기둥 칸(bay) 단위로 나눠 벽·기둥·중간 층·바닥·천장·소품 포인트를 만든다.
 * 출력 포인트마다 "Mesh"(SoftObjectPath) 속성이 있어 Static Mesh Spawner(By Attribute)로 바로 스폰한다.
 *  - 메시 후보(이 노드 설정)는 테마 그래프마다 다르게 넣는다
 *  - 방 파라미터는 실행 소스의 AEclipseInteriorVolume에서 읽고, 없으면 FallbackParams를 쓴다
 * 칸 프리팹 규칙: 폭 PrefabWidth, 피벗 = 칸 아래 가운데, 벽 면은 Y=0에서 +Y를 본다.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class ECLIPSETHENINJA_API UPCGEclipseInteriorBaysSettings : public UPCGSettings
{
    GENERATED_BODY()

public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return FName(TEXT("EclipseInteriorBays")); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGEclipseInteriorBays", "NodeTitle", "Eclipse Interior Bays"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

    // 칸 프리팹의 원래 폭. 실제 칸 폭에 맞춰 X 스케일한다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "1"))
    float PrefabWidth = 400.0f;

    // 기본 칸 뒷벽 후보
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
    TArray<FEclipseInteriorMeshEntry> WallMeshes;

    // 변형 칸 뒷벽 후보(격자창·열린 장지·족자)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
    TArray<FEclipseInteriorMeshEntry> VariantWallMeshes;

    // 칸 뒷벽 프리팹 높이. 방이 더 높으면 WallTopMesh를 쌓는다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "1"))
    float WallBaseHeight = 300.0f;

    // 벽 위를 천장까지 채우는 띠벽
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
    TSoftObjectPtr<UStaticMesh> WallTopMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "1"))
    float WallTopHeight = 100.0f;

    // 칸 경계 기둥. 방 높이에 맞춰 Z 스케일한다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Column")
    TSoftObjectPtr<UStaticMesh> ColumnMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Column", meta = (ClampMin = "1"))
    float ColumnHeight = 400.0f;

    // 뒷벽 기둥을 벽 앞으로 내미는 거리
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Column")
    float ColumnWallOffset = 12.0f;

    // 중간 층 칸 경계에도 기둥을 세운다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MidLayer")
    bool bMidLayerColumns = true;

    // 중간 층 칸마다 놓을 후보(장지 등). 빈 Mesh 후보 = 그 칸은 비움
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MidLayer")
    TArray<FEclipseInteriorMeshEntry> MidLayerBayMeshes;

    // 바닥 타일(가운데 피벗, 윗면 Z=0)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
    TSoftObjectPtr<UStaticMesh> FloorMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor", meta = (ClampMin = "1"))
    float FloorTileSize = 400.0f;

    // 바닥 타일 윗면 보정(다다미처럼 피벗 위로 두께가 있는 메시)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
    float FloorZOffset = 0.0f;

    // 천장 타일(가운데 피벗, 아랫면 Z=0)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ceiling")
    TSoftObjectPtr<UStaticMesh> CeilingMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ceiling", meta = (ClampMin = "1"))
    float CeilingTileSize = 400.0f;

    // 바닥 앞 가장자리 아래 판벽(벽 면은 Y=0에서 +Y). 방 파라미터 FrontSkirtDepth에 맞춰 Z 스케일한다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skirt")
    TSoftObjectPtr<UStaticMesh> FrontSkirtMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skirt", meta = (ClampMin = "1"))
    float FrontSkirtHeight = 300.0f;

    // 그림자 차단 상자 메시(100cm 큐브, 가운데 피벗). ShadowBlockers 핀으로 따로 나가며, 그래프에서 숨김 + 그림자만 드리우는 스포너에 연결한다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShadowBlocker")
    TSoftObjectPtr<UStaticMesh> ShadowBlockerMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));

    // 뒷벽 뒤로 띄우는 거리(열린 장지 칸 안쪽 공간보다 뒤)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShadowBlocker", meta = (ClampMin = "0"))
    float ShadowBlockerBackOffset = 260.0f;

    // 바닥 소품 후보
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Props")
    TArray<FEclipseInteriorPropEntry> Props;

    // 칸 하나의 소품 자리 수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Props", meta = (ClampMin = "0"))
    int32 PropSlotsPerBay = 2;

    // 소품 최대 yaw 흔들림(도)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Props", meta = (ClampMin = "0"))
    float PropYawJitter = 25.0f;

    // 실행 소스가 AEclipseInteriorVolume이 아닐 때 쓰는 방 파라미터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fallback")
    FEclipseInteriorRoomParams FallbackParams;

protected:
    // 입력: 방 볼륨(공간 데이터). 바운드만 쓴다
    virtual TArray<FPCGPinProperties> InputPinProperties() const override;

    // 출력: Mesh 속성이 있는 포인트
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

    virtual FPCGElementPtr CreateElement() const override;
};

class FPCGEclipseInteriorBaysElement : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;

    // 볼륨 액터 값을 읽으므로 캐시하지 않는다
    virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

    // 액터를 읽으므로 게임 스레드에서만 실행
    virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
};
