// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EclipsePCGLineActor.generated.h"

class UPCGComponent;
class UPCGGraphInterface;
class USplineComponent;

/**
 * 직선을 따라 생성하는 PCG 액터의 기반 클래스(마을 집 줄, 성벽).
 *  - 액터 위치 = 선의 시작(왼쪽 끝). 선은 액터 +X로 Length만큼 뻗는다
 *  - 길이는 Length를 입력하거나 뷰포트의 끝 핸들(마름모)을 끌어 바꾼다
 *  - 스플라인은 이 액터가 매번 다시 만든다. 스플라인 점을 직접 고치지 않는다
 *  - PCGParam 메타가 붙은 속성은 같은 이름의 그래프 파라미터로 전달된다
 */
UCLASS(Abstract, BlueprintType)
class ECLIPSETHENINJA_API AEclipsePCGLineActor : public AActor
{
    GENERATED_BODY()

public:
    AEclipsePCGLineActor();

    // 선 길이(cm)
    double GetLength() const { return this->Length; }

    // 길이를 바꾸고 다시 생성한다
    void SetLength(double NewLength);

    // 이전 생성물을 지우고 강제로 다시 생성한다
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Eclipse PCG", meta = (DisplayName = "다시 생성"))
    void Regenerate();

    // 놓거나 옮길 때 스플라인과 파라미터를 다시 만든다
    virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
    // 끝 핸들·파라미터 변경 처리
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
    // 스플라인 점(액터 로컬, 순서대로). 기본은 시작~끝 직선 두 점
    virtual void BuildLinePoints(TArray<FVector>& OutPoints) const;

    // 그래프 파라미터 전달 직후 하위 클래스가 계산값을 더 넣는 곳
    virtual void PushComputedGraphParameters() {}

    // 스플라인을 다시 만들고 파라미터를 넣는다
    void RefreshLine();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse PCG")
    TObjectPtr<USplineComponent> Spline;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse PCG")
    TObjectPtr<UPCGComponent> PCGComponent;

    // 선 길이(cm). 액터 위치에서 +X로 이만큼 생성한다. 뷰포트의 끝 핸들을 끌어도 바뀐다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse PCG|크기", meta = (DisplayName = "길이", Units = "cm", ClampMin = "100"))
    double Length = 12000.0;

    // 놓을 때 자동으로 넣는 그래프
    UPROPERTY(EditDefaultsOnly, Category = "Eclipse PCG")
    TSoftObjectPtr<UPCGGraphInterface> DefaultGraph;

#if WITH_EDITORONLY_DATA
    // 선 끝 핸들. 끌면 길이가 바뀐다(X만 반영)
    UPROPERTY(EditAnywhere, Category = "Eclipse PCG|크기", AdvancedDisplay, meta = (DisplayName = "끝 핸들", MakeEditWidget))
    FVector EndHandle;
#endif
};
