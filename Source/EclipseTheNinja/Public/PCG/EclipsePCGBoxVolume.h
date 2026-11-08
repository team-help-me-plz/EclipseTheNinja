// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGVolume.h"
#include "EclipsePCGBoxVolume.generated.h"

class UPCGGraphInterface;

/**
 * 크기를 cm로 정하는 PCG 볼륨의 기반 클래스.
 *  - 스케일은 항상 1로 둔다. 스케일 기즈모로 늘려도 Size로 옮겨 담는다
 *  - Size를 입력하거나 뷰포트의 마름모 핸들(면)을 끌어 크기를 바꾼다. 핸들을 끌면 반대쪽 면은 제자리에 있다
 *  - DefaultGraph가 있으면 놓을 때 PCG 컴포넌트에 자동으로 들어간다
 *  - 하위 클래스의 PCGParam 메타가 붙은 속성은 같은 이름의 그래프 파라미터로 전달된다
 * 사이드뷰 전제라 회전 없이(yaw 0) 놓는다.
 */
UCLASS(Abstract, BlueprintType)
class ECLIPSETHENINJA_API AEclipsePCGBoxVolume : public APCGVolume
{
    GENERATED_BODY()

public:
    AEclipsePCGBoxVolume(const FObjectInitializer& ObjectInitializer);

    // 볼륨 크기(cm)
    FVector GetBoxSize() const { return this->Size; }

    // 크기를 바꾸고 브러시를 다시 만든다
    void SetBoxSize(const FVector& NewSize);

    // 이전 생성물을 지우고 강제로 다시 생성한다
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Eclipse PCG", meta = (DisplayName = "다시 생성"))
    void Regenerate();

    // 예전 방식(스케일)으로 늘린 볼륨을 스케일 1 + Size로 바꾼다
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Eclipse PCG", meta = (DisplayName = "스케일을 크기로 변환"))
    void ConvertScaleToSize();

    // 놓거나 옮길 때 그래프·크기·브러시·파라미터를 맞춘다
    virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
    // 크기·핸들·파라미터 변경 처리
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

    // 스케일 기즈모로 늘린 값을 Size로 옮긴다
    virtual void PostEditMove(bool bFinished) override;
#endif

protected:
    // 그래프 파라미터 전달 직후 하위 클래스가 계산값(볼륨 크기에서 나오는 범위 등)을 더 넣는 곳
    virtual void PushComputedGraphParameters() {}

    // 노드가 액터 값을 직접 읽는 볼륨이면 true. 속성이 바뀔 때마다 다시 생성한다
    virtual bool ReadsActorValuesDirectly() const { return false; }

    // PCGParam 속성과 계산값을 그래프 인스턴스에 넣는다
    void PushGraphParameters();

    // 그래프가 비어 있으면 DefaultGraph를 넣는다
    void EnsureDefaultGraph();

    // 볼륨 크기(cm). X = 가로 길이, Y = 앞뒤 깊이, Z = 높이. 뷰포트의 마름모 핸들을 끌어도 바뀐다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse PCG|크기", meta = (DisplayName = "크기", Units = "cm", ClampMin = "10"))
    FVector Size = FVector(2000.0, 2000.0, 400.0);

    // 놓을 때 자동으로 넣는 그래프
    UPROPERTY(EditDefaultsOnly, Category = "Eclipse PCG")
    TSoftObjectPtr<UPCGGraphInterface> DefaultGraph;

#if WITH_EDITORONLY_DATA
    // 오른쪽(+X) 면 핸들. 끌면 왼쪽 면은 고정된 채 길이가 바뀐다
    UPROPERTY(EditAnywhere, Category = "Eclipse PCG|크기", AdvancedDisplay, meta = (DisplayName = "핸들 +X", MakeEditWidget))
    FVector HandleXMax;

    // 왼쪽(-X) 면 핸들
    UPROPERTY(EditAnywhere, Category = "Eclipse PCG|크기", AdvancedDisplay, meta = (DisplayName = "핸들 -X", MakeEditWidget))
    FVector HandleXMin;

    // 카메라 쪽(+Y) 면 핸들
    UPROPERTY(EditAnywhere, Category = "Eclipse PCG|크기", AdvancedDisplay, meta = (DisplayName = "핸들 +Y(앞)", MakeEditWidget))
    FVector HandleYMax;

    // 뒤쪽(-Y) 면 핸들
    UPROPERTY(EditAnywhere, Category = "Eclipse PCG|크기", AdvancedDisplay, meta = (DisplayName = "핸들 -Y(뒤)", MakeEditWidget))
    FVector HandleYMin;

    // 윗면 핸들. 아랫면은 고정
    UPROPERTY(EditAnywhere, Category = "Eclipse PCG|크기", AdvancedDisplay, meta = (DisplayName = "핸들 위", MakeEditWidget))
    FVector HandleZMax;
#endif

private:
    // 브러시가 Size와 다르면 다시 만든다
    void RebuildBrushIfNeeded();

    // 핸들을 현재 면 위치로 되돌린다
    void ResetHandles();

#if WITH_EDITOR
    // 끌린 핸들만큼 그 면을 옮긴다. 핸들 속성이었으면 true
    bool ApplyHandleDrag(FName PropertyName);
#endif

    // 브러시 재생성 중 다시 들어오는 편집 이벤트를 막는다
    bool bRebuildingBrush = false;
};
