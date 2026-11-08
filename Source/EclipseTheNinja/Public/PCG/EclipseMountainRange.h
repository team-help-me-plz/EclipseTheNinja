// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EclipseMountainRange.generated.h"

class UDynamicMeshComponent;
class UMaterialInterface;

// 산 능선 한 겹
USTRUCT(BlueprintType)
struct FEclipseMountainLayer
{
    GENERATED_BODY()

    // 이 겹을 만든다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "산맥", meta = (DisplayName = "사용"))
    bool bEnabled = true;

    // 액터 위치에서 뒤(−Y)로 떨어진 거리(cm). 뒤 겹일수록 크게
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "산맥", meta = (DisplayName = "뒤로 거리", Units = "cm", ClampMin = "0"))
    float Distance = 0.0f;

    // 능선 골짜기 높이(cm). 능선이 이 아래로는 내려가지 않는다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "산맥", meta = (DisplayName = "최저 높이", Units = "cm", ClampMin = "0"))
    float BaseHeight = 2000.0f;

    // 가장 높은 봉우리 높이(cm). 화면에서 산이 차지하는 위쪽 끝을 정한다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "산맥", meta = (DisplayName = "최고 높이", Units = "cm", ClampMin = "0"))
    float PeakHeight = 8000.0f;

    // 봉우리 간격(cm). 작을수록 봉우리가 촘촘하고 뾰족하다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "산맥", meta = (DisplayName = "봉우리 간격", Units = "cm", ClampMin = "1000"))
    float PeakSpacing = 18000.0f;

    // 0~1. 능선의 잔 굴곡. 높이면 능선이 울퉁불퉁해진다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "산맥", meta = (DisplayName = "거칠기", ClampMin = "0", ClampMax = "1"))
    float Roughness = 0.35f;

    // 산 색. 뒤 겹일수록 밝고 푸르게 하면 공기 원근감이 난다(포그가 더 덮는다)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "산맥", meta = (DisplayName = "색"))
    FLinearColor Color = FLinearColor(0.12f, 0.14f, 0.16f);
};

/**
 * 먼 배경 산맥. 노이즈로 만든 능선 메시를 여러 겹(최대 3) 세운다. 외부 에셋 없이 동작한다.
 *  - 액터 위치 = 맨 앞 겹의 왼쪽 끝 바닥. 액터 +X로 Length만큼, 뒤(−Y)로 겹마다 Distance만큼
 *  - 콜리전·그림자 없음(배경 전용). 포그가 덮어 뒤 겹일수록 옅어진다
 *  - 값을 바꾸면 바로 다시 만든다. 시드만 바꾸면 같은 높이로 다른 능선
 */
UCLASS(DisplayName = "Eclipse 산맥")
class ECLIPSETHENINJA_API AEclipseMountainRange : public AActor
{
    GENERATED_BODY()

public:
    AEclipseMountainRange();

    // 놓거나 옮길 때 능선 메시를 다시 만든다
    virtual void OnConstruction(const FTransform& Transform) override;

    // 능선 메시를 다시 만든다
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "산맥", meta = (DisplayName = "다시 생성"))
    void Rebuild();

#if WITH_EDITOR
    // 끝 핸들·값 변경 처리
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
    // 겹 하나의 능선 메시를 컴포넌트에 넣는다
    void BuildLayer(int32 LayerIndex);

    // 무작위 시드. 능선 모양이 바뀐다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "산맥", meta = (DisplayName = "시드"))
    int32 Seed = 0;

    // 산맥 가로 길이(cm). 플레이 구간보다 훨씬 넓게(먼 배경은 화면에 넓게 보인다)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "산맥", meta = (DisplayName = "길이", Units = "cm", ClampMin = "1000"))
    double Length = 120000.0;

    // 능선 앞뒤 두께(cm). 사이드뷰에서는 거의 안 보이지만 너무 얇으면 비스듬한 면이 어둡다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "산맥", meta = (DisplayName = "두께", Units = "cm", ClampMin = "100"))
    double Thickness = 8000.0;

    // 가로 격자 간격(cm). 작을수록 능선이 매끄럽지만 무겁다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "산맥", meta = (DisplayName = "격자 간격", Units = "cm", ClampMin = "100"))
    double GridStep = 600.0;

    // 능선 겹(앞 → 뒤). 최대 3개
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "산맥", meta = (DisplayName = "겹", EditFixedSize))
    TArray<FEclipseMountainLayer> RidgeLayers;

    // 산 재질. Color 벡터 파라미터가 있어야 겹 색이 들어간다(기본: 엔진 BasicShapeMaterial)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "산맥", meta = (DisplayName = "재질"))
    TObjectPtr<UMaterialInterface> Material;

    UPROPERTY(VisibleAnywhere, Category = "산맥")
    TArray<TObjectPtr<UDynamicMeshComponent>> LayerComponents;

#if WITH_EDITORONLY_DATA
    // 오른쪽 끝 핸들. 끌면 길이가 바뀐다(X만 반영)
    UPROPERTY(EditAnywhere, Category = "산맥", AdvancedDisplay, meta = (DisplayName = "끝 핸들", MakeEditWidget))
    FVector EndHandle;
#endif
};
