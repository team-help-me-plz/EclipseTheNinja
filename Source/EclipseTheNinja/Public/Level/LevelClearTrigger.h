// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameFramework/Actor.h"
#include "LevelClearTrigger.generated.h"

class UBoxComponent;
class UGameEndCountdownWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLevelCleared);

/**
 * 레벨 클리어 트리거 액터.
 *  - ClearTrigger : 플레이어가 닿으면 레벨 클리어 처리
 *  - 클리어 시 플레이어 입력을 막고, TravelDelay 후 NextLevel로 이동한다
 *  - NextLevel은 레벨에 배치한 인스턴스의 디테일 패널에서 맵 에셋을 골라 설정
 *  - bQuitGameOnClear를 켜면 레벨 이동 대신 카운트다운 메시지를 띄운 뒤 게임을 종료한다
 */
UCLASS()
class ECLIPSETHENINJA_API ALevelClearTrigger : public AActor
{
    GENERATED_BODY()

public:
    ALevelClearTrigger();

    // 클리어 처리 후 다음 레벨로 이동(또는 게임 종료)하는 함수. 이미 클리어되었으면 무시
    UFUNCTION(BlueprintCallable, Category = "LevelClear")
    void ClearLevel();

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

    // 게임 종료 카운트다운을 시작하는 함수 (메시지 표시, 게임 일시정지)
    void StartQuitCountdown(APlayerController* PlayerController);

    // 1초마다 호출되어 카운트를 줄이고, 0이 되면 게임을 종료하는 함수
    bool TickQuitCountdown(float DeltaTime);

    // 게임을 종료하는 함수
    void QuitGame();

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
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear", meta = (EditCondition = "!bQuitGameOnClear"))
    TSoftObjectPtr<UWorld> NextLevel = nullptr;

    // 클리어 후 다음 레벨로 이동하기까지의 대기 시간(초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear", meta = (ClampMin = "0.0", EditCondition = "!bQuitGameOnClear"))
    float TravelDelay = 1.0f;

    // 켜면 레벨 이동 대신 카운트다운 후 게임을 종료한다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear|Quit")
    bool bQuitGameOnClear = false;

    // 게임 종료까지 세는 초
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear|Quit", meta = (ClampMin = "1", EditCondition = "bQuitGameOnClear"))
    int32 QuitCountdownSeconds = 5;

    // 카운트다운 중 게임을 일시정지할지 여부
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear|Quit", meta = (EditCondition = "bQuitGameOnClear"))
    bool bPauseGameOnQuitCountdown = true;

    // 카운트다운 안내 메시지. {0}에 QuitCountdownSeconds가 들어간다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear|Quit", meta = (EditCondition = "bQuitGameOnClear"))
    FText QuitMessageFormat = NSLOCTEXT("LevelClearTrigger", "QuitMessage", "목표에 도달했습니다. {0}초 후 게임이 종료됩니다.");

private:
    bool bIsCleared = false;

    FTimerHandle TravelTimerHandle;

    // 카운트다운에 남은 초
    int32 RemainingQuitSeconds = 0;

    // 일시정지 중에도 돌아가도록 월드 타이머 대신 코어 티커를 사용
    FTSTicker::FDelegateHandle QuitTickerHandle;

    UPROPERTY(Transient)
    TObjectPtr<UGameEndCountdownWidget> CountdownWidget = nullptr;

};
