// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ObjectPoolTypes.generated.h"

// 풀이 최대 수량(MaxSize)에 도달했는데 대기 중인 액터가 없을 경우의 처리 방식
UENUM(BlueprintType)
enum class EObjectPoolOverflowPolicy : uint8
{
    // 최대 수량을 넘어서 새로 생성하고 풀에 편입한다
    Grow        UMETA(DisplayName = "Grow"),

    // 생성하지 않는다 (Spawn이 nullptr를 반환)
    DoNotSpawn  UMETA(DisplayName = "Do Not Spawn"),

    // 사용 중인 액터 중 가장 오래된 것을 강제로 반환시킨 뒤 재사용한다
    ReuseOldest UMETA(DisplayName = "Reuse Oldest"),
};
