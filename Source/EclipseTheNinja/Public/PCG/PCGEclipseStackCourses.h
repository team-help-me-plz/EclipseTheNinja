// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGSettings.h"
#include "PCGEclipseStackCourses.generated.h"

/**
 * 석축처럼 피벗이 윗면인 조각을 높이만큼 여러 단으로 쌓는다(늘이지 않고 단을 늘린다).
 * 입력 포인트 하나 = 맨 윗단 위치. 단 수 = 올림(높이 배율 / 단 최대 배율), 단마다 Z 배율 = 높이 배율 / 단 수.
 * 아랫단은 단 높이만큼 내려가고, 단마다 CourseOffset(포인트 로컬)만큼 더 밀린다(기울어진 석축 면을 잇는 용도).
 * 속성(Mesh 등)은 원래 포인트 것을 그대로 쓴다.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class ECLIPSETHENINJA_API UPCGEclipseStackCoursesSettings : public UPCGSettings
{
    GENERATED_BODY()

public:
#if WITH_EDITOR
    // 그래프에 추가될 때의 노드 이름
    virtual FName GetDefaultNodeName() const override { return FName(TEXT("EclipseStackCourses")); }

    // 그래프 에디터에 보이는 노드 제목
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGEclipseStackCourses", "NodeTitle", "Eclipse Stack Courses"); }

    // 노드 색·분류(포인트 연산)
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
#endif

    // 전체 높이 배율(1 = 한 단 높이). 그래프의 WallHeightScale 파라미터를 연결한다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stack", meta = (PCG_Overridable, ClampMin = "0.01"))
    double HeightScale = 1.0;

    // 조각 한 단의 원래 높이(cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stack", meta = (PCG_Overridable, ClampMin = "1"))
    double CourseHeight = 400.0;

    // 한 단이 늘어날 수 있는 최대 배율. 넘으면 단을 하나 더 쌓는다. 1.3이면 높이 4.5배 → 4단(단마다 1.125배)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stack", meta = (PCG_Overridable, ClampMin = "1"))
    double MaxCourseScale = 1.3;

    // 한 단 내려갈 때마다 더하는 위치(포인트 로컬, cm). 기울어진 석축이면 바깥쪽(+Y 등)으로 민다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stack", meta = (PCG_Overridable))
    FVector CourseOffset = FVector::ZeroVector;

protected:
    // 입력: 맨 윗단 포인트
    virtual TArray<FPCGPinProperties> InputPinProperties() const override;

    // 출력: 단마다 하나씩 늘어난 포인트
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

    // 실행 요소 생성
    virtual FPCGElementPtr CreateElement() const override;
};

class FPCGEclipseStackCoursesElement : public IPCGElement
{
protected:
    // 포인트를 단 수만큼 복제해 내린다
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
