// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGSettings.h"
#include "PCGEclipseGround.generated.h"

/**
 * 바닥 볼륨(AEclipseGroundVolume)을 타일 격자와 흩뿌림 포인트로 바꾼다.
 * 출력 포인트마다 "Mesh"(SoftObjectPath) 속성이 있어 Static Mesh Spawner(By Attribute)로 바로 스폰한다.
 * 설정은 실행 소스 액터에서 읽으므로 노드 자체 설정은 없다. 바닥 볼륨이 아니면 아무것도 내지 않는다.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class ECLIPSETHENINJA_API UPCGEclipseGroundSettings : public UPCGSettings
{
    GENERATED_BODY()

public:
#if WITH_EDITOR
    // 그래프에 추가될 때의 노드 이름
    virtual FName GetDefaultNodeName() const override { return FName(TEXT("EclipseGround")); }

    // 그래프 에디터에 보이는 노드 제목
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGEclipseGround", "NodeTitle", "Eclipse Ground"); }

    // 노드 색·분류(샘플러)
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

protected:
    // 입력: 바닥 볼륨(연결하지 않아도 된다. 실행 소스 액터를 읽는다)
    virtual TArray<FPCGPinProperties> InputPinProperties() const override;

    // 출력: Mesh 속성이 있는 포인트
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

    // 실행 요소 생성
    virtual FPCGElementPtr CreateElement() const override;
};

class FPCGEclipseGroundElement : public IPCGElement
{
protected:
    // 타일·흩뿌림 포인트를 만든다
    virtual bool ExecuteInternal(FPCGContext* Context) const override;

    // 볼륨 액터 값을 읽으므로 캐시하지 않는다
    virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

    // 액터·메시를 읽으므로 게임 스레드에서만 실행
    virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
};
