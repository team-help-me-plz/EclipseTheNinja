// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "ObjectPoolLevelConfig.generated.h"

class UObjectPoolDataAsset;

/**
 * AObjectPoolLevelConfig가 레벨 시작 시 오브젝트 풀에 보내는 사전 생성 요청 구조체.
 *  - 요청은 Config의 BeginPlay에서 보내지고, 실제 생성은 모든 액터의 BeginPlay가 끝난 뒤 이루어진다
 *    (이미 플레이 중인 월드에 스트리밍으로 로드된 경우에는 즉시 생성)
 *  - 수량은 다른 요청(bWarmupInAllLevels, 스포너, 다른 항목)과 합산되며, 합계는 풀의 MaxSize(0이면 무제한)를 넘지 않는다
 *  - 풀은 줄어들지 않으므로 이미 그만큼 만들어져 있다면 추가로 생성하지 않는다
 */
USTRUCT(BlueprintType)
struct FObjectPoolWarmupEntry
{
    GENERATED_BODY()

    // 미리 생성할 풀의 데이터 에셋. 비어 있으면 무시된다
    //  - 프로젝트 세팅에 없는 에셋이면 이 에셋의 설정으로 이 레벨(월드)에서만 풀이 등록된다
    //  - 같은 ActorClass의 풀이 이미 등록되어 있으면 기존 풀 설정(InitialSize, MaxSize, OverflowPolicy)을 따르고 이 에셋의 설정은 쓰지 않는다
    //  - 하드 레퍼런스이므로 레벨 로드 시 에셋과 ActorClass가 함께 로드된다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ObjectPool")
    TObjectPtr<UObjectPoolDataAsset> DataAsset = nullptr;

    // 이 레벨에서 추가로 요청할 수량
    //  - 음수 : 풀의 InitialSize만큼 요청 (기본값 -1)
    //  - 0   : 미리 생성하지 않고 풀 등록만 한다 (레벨 전용 풀을 처음 스폰될 때 생성하고 싶을 때)
    //  - 양수 : 그 수량만큼 요청
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ObjectPool")
    int32 WarmupCount = -1;
};

/**
 * 레벨에 배치해, 이 레벨에서 미리 생성할 풀을 지정하는 액터.
 * 프로젝트 세팅에 없는 데이터 에셋도 지정할 수 있으며, 이 경우 해당 레벨에서만 풀이 등록된다.
 * World Partition 레벨에서도 항상 로드되도록 공간 로딩을 끈다.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Object Pool Level Config"))
class ECLIPSETHENINJA_API AObjectPoolLevelConfig : public AInfo
{
    GENERATED_BODY()

public:
    AObjectPoolLevelConfig();

protected:
    virtual void BeginPlay() override;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ObjectPool")
    TArray<FObjectPoolWarmupEntry> WarmupEntries;

};
