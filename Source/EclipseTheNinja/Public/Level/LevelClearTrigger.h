// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelClearTrigger.generated.h"

class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLevelCleared);

/**
 * 레벨 클리어 트리거 액터.
 *  - ClearTrigger : 플레이어가 닿으면 레벨 클리어 처리
 *  - 클리어 시 플레이어 입력을 막고, TravelDelay 후 NextLevel로 이동한다
 *  - NextLevel은 레벨에 배치한 인스턴스의 디테일 패널에서 맵 에셋을 골라 설정
 */
UCLASS()
class ECLIPSETHENINJA_API ALevelClearTrigger : public AActor
{
    GENERATED_BODY()

public:
    ALevelClearTrigger();

    // 클리어 처리 후 다음 레벨로 이동하는 함수. 이미 클리어되었으면 무시
    UFUNCTION(BlueprintCallable, Category = "LevelClear")
    void ClearLevel();

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    // NextLevel을 여는 함수
    void TravelToNextLevel();

public:
    // 레벨 클리어 시 실행되는 델리게이트 (클리어 UI, 사운드 등이 사용)
    UPROPERTY(BlueprintAssignable, Category = "LevelClear")
    FOnLevelCleared OnLevelCleared;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> ClearTrigger = nullptr;

    // 클리어 후 이동할 레벨. 비어 있으면 클리어 처리만 하고 이동하지 않는다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear")
    TSoftObjectPtr<UWorld> NextLevel = nullptr;

    // 클리어 후 다음 레벨로 이동하기까지의 대기 시간(초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear", meta = (ClampMin = "0.0"))
    float TravelDelay = 1.0f;

private:
    bool bIsCleared = false;

    FTimerHandle TravelTimerHandle;

};
