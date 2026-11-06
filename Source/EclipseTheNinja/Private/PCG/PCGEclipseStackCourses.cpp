// Fill out your copyright notice in the Description page of Project Settings.

#include "PCG/PCGEclipseStackCourses.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h"

TArray<FPCGPinProperties> UPCGEclipseStackCoursesSettings::InputPinProperties() const
{
    TArray<FPCGPinProperties> PinProperties;
    PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
    return PinProperties;
}

TArray<FPCGPinProperties> UPCGEclipseStackCoursesSettings::OutputPinProperties() const
{
    TArray<FPCGPinProperties> PinProperties;
    PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
    return PinProperties;
}

FPCGElementPtr UPCGEclipseStackCoursesSettings::CreateElement() const
{
    return MakeShared<FPCGEclipseStackCoursesElement>();
}

bool FPCGEclipseStackCoursesElement::ExecuteInternal(FPCGContext* Context) const
{
    const UPCGEclipseStackCoursesSettings* Settings = Context->GetInputSettings<UPCGEclipseStackCoursesSettings>();
    check(Settings);

    // 단 수와 단마다의 Z 배율
    const double HeightScale = FMath::Max(Settings->HeightScale, 0.01);
    const double MaxCourseScale = FMath::Max(Settings->MaxCourseScale, 1.0);
    const int32 NumCourses = FMath::Max(1, FMath::CeilToInt32(HeightScale / MaxCourseScale - KINDA_SMALL_NUMBER));
    const double CourseScale = HeightScale / NumCourses;
    const double CourseStep = Settings->CourseHeight * CourseScale;

    for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
    {
        const UPCGBasePointData* InPoints = Cast<UPCGBasePointData>(Input.Data);
        if (!InPoints)
        {
            continue;
        }

        // 포인트마다 단 수만큼 같은 원본 인덱스를 넣어 복제한다(속성도 같이 따라온다)
        const int32 NumIn = InPoints->GetNumPoints();
        TArray<int32> SourceIndices;
        SourceIndices.Reserve(NumIn * NumCourses);
        for (int32 Index = 0; Index < NumIn; ++Index)
        {
            for (int32 Course = 0; Course < NumCourses; ++Course)
            {
                SourceIndices.Add(Index);
            }
        }

        // 메타데이터(Mesh 등 속성)를 이어받도록 원본으로 먼저 초기화한 뒤 포인트를 복제한다
        UPCGBasePointData* OutPoints = FPCGContext::NewPointData_AnyThread(Context);
        OutPoints->InitializeFromDataWithParams(FPCGInitializeFromDataParams(InPoints));
        OutPoints->SetPointsFrom(InPoints, SourceIndices);

        // 단 k는 k단 높이만큼 내리고 로컬 오프셋을 k번 더한다
        TPCGValueRange<FTransform> Transforms = OutPoints->GetTransformValueRange();
        for (int32 OutIndex = 0; OutIndex < Transforms.Num(); ++OutIndex)
        {
            const int32 Course = OutIndex % NumCourses;
            FTransform& Transform = Transforms[OutIndex];

            const FVector LocalOffset = Settings->CourseOffset * Course;
            const FVector Location = Transform.GetLocation()
                + Transform.GetRotation().RotateVector(LocalOffset)
                - FVector(0.0, 0.0, CourseStep * Course);

            FVector Scale = Transform.GetScale3D();
            Scale.Z *= CourseScale;

            Transform.SetLocation(Location);
            Transform.SetScale3D(Scale);
        }

        FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
        Output.Data = OutPoints;
    }

    return true;
}
