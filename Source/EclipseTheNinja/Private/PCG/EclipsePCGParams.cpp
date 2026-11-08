// Fill out your copyright notice in the Description page of Project Settings.

#include "EclipsePCGParams.h"

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "Helpers/PCGGraphParametersHelpers.h"
#include "StructUtils/PropertyBag.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogEclipsePCGParams, Log, All);

namespace EclipsePCGParams
{
    const FName ParamMetaKey = TEXT("PCGParam");

    // 컴포넌트의 그래프 인스턴스. 그래프가 없으면 null
    UPCGGraphInstance* GetInstance(UPCGComponent* Component)
    {
        if (!Component)
        {
            return nullptr;
        }

        UPCGGraphInstance* Instance = Component->GetGraphInstance();
        if (!Instance || !Instance->GetGraph())
        {
            return nullptr;
        }

        return Instance;
    }

    // 그래프 파라미터 설명. 없으면 (그래프, 이름)마다 한 번만 경고를 남기고 null
    const FPropertyBagPropertyDesc* FindDesc(UPCGGraphInstance* Instance, FName Name)
    {
        const FInstancedPropertyBag* Bag = Instance->GetUserParametersStruct();
        const FPropertyBagPropertyDesc* Desc = Bag ? Bag->FindPropertyDescByName(Name) : nullptr;

        // 볼륨을 끌 때 매 틱 불리므로 같은 경고를 반복하지 않는다
        static TSet<FString> WarnedKeys;
        const FString Key = GetNameSafe(Instance->GetGraph()) + TEXT(".") + Name.ToString();

        bool bAlreadyWarned = false;
        if (!Desc)
        {
            WarnedKeys.Add(Key, &bAlreadyWarned);
        }

        if (!Desc && !bAlreadyWarned)
        {
            UE_LOG(
                LogEclipsePCGParams,
                Warning,
                TEXT("그래프 %s에 파라미터 %s가 없습니다"),
                *GetNameSafe(Instance->GetGraph()),
                *Name.ToString()
            );
        }

        return Desc;
    }

    bool IsVectorDesc(const FPropertyBagPropertyDesc& Desc)
    {
        return Desc.ValueType == EPropertyBagPropertyType::Struct
            && Desc.ValueTypeObject == TBaseStructure<FVector>::Get();
    }

    void SetVectorChecked(UPCGGraphInstance* Instance, FName Name, const FVector& Value)
    {
        if (!UPCGGraphParametersHelpers::GetVectorParameter(Instance, Name).Equals(Value, 1e-3))
        {
            UPCGGraphParametersHelpers::SetVectorParameter(Instance, Name, Value);
        }
    }

    void SetNumber(UPCGComponent* Component, FName Name, double Value)
    {
        UPCGGraphInstance* Instance = GetInstance(Component);
        const FPropertyBagPropertyDesc* Desc = Instance ? FindDesc(Instance, Name) : nullptr;
        if (!Desc)
        {
            return;
        }

        // 그래프 쪽 타입에 맞춰 변환해 넣는다
        switch (Desc->ValueType)
        {
        case EPropertyBagPropertyType::Int32:
        {
            const int32 IntValue = FMath::RoundToInt32(Value);
            if (UPCGGraphParametersHelpers::GetInt32Parameter(Instance, Name) != IntValue)
            {
                UPCGGraphParametersHelpers::SetInt32Parameter(Instance, Name, IntValue);
            }
            break;
        }
        case EPropertyBagPropertyType::Int64:
        {
            const int64 IntValue = FMath::RoundToInt64(Value);
            if (UPCGGraphParametersHelpers::GetInt64Parameter(Instance, Name) != IntValue)
            {
                UPCGGraphParametersHelpers::SetInt64Parameter(Instance, Name, IntValue);
            }
            break;
        }
        case EPropertyBagPropertyType::Float:
        {
            if (!FMath::IsNearlyEqual(UPCGGraphParametersHelpers::GetFloatParameter(Instance, Name), static_cast<float>(Value), 1e-4f))
            {
                UPCGGraphParametersHelpers::SetFloatParameter(Instance, Name, static_cast<float>(Value));
            }
            break;
        }
        case EPropertyBagPropertyType::Double:
        {
            if (!FMath::IsNearlyEqual(UPCGGraphParametersHelpers::GetDoubleParameter(Instance, Name), Value, 1e-4))
            {
                UPCGGraphParametersHelpers::SetDoubleParameter(Instance, Name, Value);
            }
            break;
        }
        case EPropertyBagPropertyType::Bool:
        {
            SetBool(Component, Name, Value != 0.0);
            break;
        }
        default:
        {
            // 크기 배율처럼 벡터 파라미터에 숫자 하나를 넣으면 균일 벡터로 넣는다
            if (IsVectorDesc(*Desc))
            {
                SetVectorChecked(Instance, Name, FVector(Value));
            }
            else
            {
                UE_LOG(LogEclipsePCGParams, Warning, TEXT("파라미터 %s의 타입이 숫자가 아닙니다"), *Name.ToString());
            }
            break;
        }
        }
    }

    void SetBool(UPCGComponent* Component, FName Name, bool bValue)
    {
        UPCGGraphInstance* Instance = GetInstance(Component);
        if (!Instance || !FindDesc(Instance, Name))
        {
            return;
        }

        if (UPCGGraphParametersHelpers::GetBoolParameter(Instance, Name) != bValue)
        {
            UPCGGraphParametersHelpers::SetBoolParameter(Instance, Name, bValue);
        }
    }

    void SetVector(UPCGComponent* Component, FName Name, const FVector& Value)
    {
        UPCGGraphInstance* Instance = GetInstance(Component);
        if (!Instance || !FindDesc(Instance, Name))
        {
            return;
        }

        SetVectorChecked(Instance, Name, Value);
    }

    void SetSoftPath(UPCGComponent* Component, FName Name, const FSoftObjectPath& Value)
    {
        UPCGGraphInstance* Instance = GetInstance(Component);
        const FPropertyBagPropertyDesc* Desc = Instance ? FindDesc(Instance, Name) : nullptr;
        if (!Desc)
        {
            return;
        }

        if (Desc->ValueType == EPropertyBagPropertyType::SoftObject)
        {
            const TSoftObjectPtr<UObject> Ptr(Value);
            if (UPCGGraphParametersHelpers::GetSoftObjectParameter(Instance, Name) != Ptr)
            {
                UPCGGraphParametersHelpers::SetSoftObjectParameter(Instance, Name, Ptr);
            }
            return;
        }

        if (UPCGGraphParametersHelpers::GetSoftObjectPathParameter(Instance, Name) != Value)
        {
            UPCGGraphParametersHelpers::SetSoftObjectPathParameter(Instance, Name, Value);
        }
    }

    void PushReflected(const UObject* Owner, UPCGComponent* Component)
    {
#if WITH_EDITOR
        if (!Owner || !GetInstance(Component))
        {
            return;
        }

        for (TFieldIterator<FProperty> It(Owner->GetClass()); It; ++It)
        {
            const FProperty* Property = *It;
            if (!Property->HasMetaData(ParamMetaKey))
            {
                continue;
            }

            // 메타 값이 있으면 그 이름, 없으면 속성 이름 그대로
            const FString& MetaName = Property->GetMetaData(ParamMetaKey);
            const FName ParamName = MetaName.IsEmpty()
                ? Property->GetFName()
                : FName(*MetaName);

            const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);

            if (const FIntProperty* IntProperty = CastField<FIntProperty>(Property))
            {
                SetNumber(Component, ParamName, IntProperty->GetPropertyValue(ValuePtr));
            }
            else if (const FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
            {
                SetNumber(Component, ParamName, FloatProperty->GetPropertyValue(ValuePtr));
            }
            else if (const FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
            {
                SetNumber(Component, ParamName, DoubleProperty->GetPropertyValue(ValuePtr));
            }
            else if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
            {
                SetBool(Component, ParamName, BoolProperty->GetPropertyValue(ValuePtr));
            }
            else if (const FSoftObjectProperty* SoftProperty = CastField<FSoftObjectProperty>(Property))
            {
                SetSoftPath(Component, ParamName, SoftProperty->GetPropertyValue(ValuePtr).ToSoftObjectPath());
            }
            else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
            {
                if (StructProperty->Struct == TBaseStructure<FVector>::Get())
                {
                    SetVector(Component, ParamName, *static_cast<const FVector*>(ValuePtr));
                }
            }
        }
#endif
    }
}
