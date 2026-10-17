// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Poolable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UPoolable : public UInterface
{
    GENERATED_BODY()
};

/**
 * 오브젝트 풀에서 재사용되는 액터가 자신만의 상태를 초기화/정리해야 할 때 상속 및 구현한다.
 * 구현하지 않아도 풀링은 동작하며(오브젝트 풀에 등록된 경우), UObjectPoolSubsystem이 다음의 공통 처리를 항상 수행한다.
 *  - 반환 시 : 숨김, 콜리전/액터 틱 끄기, 활성 컴포넌트 Deactivate, 액터 타이머 정리, 대기 위치로 이동
 *  - 스폰 시 : 숨김/콜리전/틱/컴포넌트 활성 상태를 클래스 기본값으로 복원
 * 그 외의 상태(체력, AI 재시작, 발사체 속도 등)는 여기서 직접 처리한다.
 */
class ECLIPSETHENINJA_API IPoolable
{
    GENERATED_BODY()

public:
    // 풀에서 꺼내질 때마다 호출된다.
    // 처음 생성된 경우에도 호출되며, 호출 시점은 공통 활성화 처리 이후
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ObjectPool")
    void OnSpawnFromPool();

    // 풀로 반환될 때마다 호출된다.
    // 처음 생성되어 풀에 들어갈 때(BeginPlay 직후)에도 호출되며, 호출 시점은 공통 비활성화 처리 이전
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ObjectPool")
    void OnReturnToPool();
};
