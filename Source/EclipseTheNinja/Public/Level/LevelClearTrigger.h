// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameFramework/Actor.h"
#include "Checkpoint/CheckpointId.h"
#include "LevelClearTrigger.generated.h"

class UBoxComponent;
class UGameEndCountdownWidget;
class AEnemy;
class ABossEnemy;
class ATutorialSequenceTrigger;
class ULevelSequence;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLevelCleared);

/**
 * 레벨 클리어 트리거 액터.
 *  - ClearTrigger : 플레이어가 닿으면 레벨 클리어 처리
 *  - 클리어 시 플레이어 입력을 막고, TravelDelay 후 NextLevel로 이동한다
 *  - NextLevel은 레벨에 배치한 인스턴스의 디테일 패널에서 맵 에셋을 골라 설정
 *  - bQuitGameOnClear를 켜면 레벨 이동 대신 카운트다운 메시지를 띄운 뒤 게임을 종료한다
 *  - EndingSequence를 지정하면 클리어 직후 엔딩 컷신을 플레이어 위치 기준으로 재생하고, 끝난 뒤에 이동·종료를 이어 간다
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

    // 지정한 보스의 사망 연출이 끝나면 기존 클리어/종료 흐름을 실행한다.
    void OnBossDeathFinished(AEnemy* Enemy);

    // 보스를 처치한 순간 플레이어가 보던 방향을 기억한다. 사망 연출이 끝나 클리어할 때는 이미 돌아서 있을 수 있다
    void OnBossDied(AEnemy* Enemy, bool bAssassinated);

    UFUNCTION()
    void OnTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    // 엔딩이 끝나면(또는 엔딩이 없으면) 다음 레벨 이동이나 게임 종료 카운트다운을 시작하는 함수
    void FinishClear();

    // 엔딩 컷신이 끝 프레임에 도달하면 호출된다
    UFUNCTION()
    void OnEndingSequenceEnded(ATutorialSequenceTrigger* Trigger);

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
    // 지정하면 도착 지점 오버랩 대신 이 보스의 처치 완료로 클리어한다.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LevelClear|Boss")
    TObjectPtr<ABossEnemy> BossToDefeat = nullptr;

    // 클리어 직후 재생할 엔딩 컷신. 원점 근처에 만든 시퀀스를 플레이어 발밑 기준으로 재생한다
    // 비워 두거나 재생할 수 없으면 바로 이동·종료로 넘어간다
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LevelClear|Ending")
    TObjectPtr<ATutorialSequenceTrigger> EndingSequence = nullptr;

    // EndingSequence의 시퀀스는 플레이어가 +X(오른쪽)를 볼 때 기준으로 만든다.
    // 클리어 순간 플레이어가 -X(왼쪽)를 보고 있으면 대신 이 좌우 대칭 시퀀스를 재생해, 원래 바라보던 방향을 그대로 잇는다
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LevelClear|Ending")
    TObjectPtr<ULevelSequence> EndingSequenceFacingLeft = nullptr;

    // 오른쪽을 볼 때 재생할 원래 시퀀스. 처음 클리어할 때 EndingSequence 액터에 지정된 시퀀스를 기억해 둔다
    UPROPERTY(Transient)
    TObjectPtr<ULevelSequence> EndingSequenceFacingRight = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> ClearTrigger = nullptr;

    // 클리어 후 이동할 레벨. 비어 있으면 클리어 처리만 하고 이동하지 않는다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear", meta = (EditCondition = "!bQuitGameOnClear"))
    TSoftObjectPtr<UWorld> NextLevel = nullptr;

    // 다음 레벨을 열 때 넘길 옵션 문자열 (예: "?Spawn=CastleGate?Stage=2"). 다음 레벨의 GameMode에서 OptionsString으로 읽는다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelClear", meta = (EditCondition = "!bQuitGameOnClear"))
    FString TravelOptions;

    // 다음 레벨에서 이 체크포인트 위치로 시작한다. None이면 그 레벨의 시작 위치(PlayerStart·시작 체크포인트)
    // 예: 성 안에서 성 밖으로 되돌아갈 때 성 밖의 "성 안 문 앞" 체크포인트
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear", meta = (EditCondition = "!bQuitGameOnClear"))
    ECheckpointId ArrivalCheckpoint = ECheckpointId::None;

    // 끄면 이 맵을 클리어 목록에 넣지 않는다. 앞 맵으로 되돌아가는 출구에서 끈다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear", meta = (EditCondition = "!bQuitGameOnClear"))
    bool bMarkLevelCleared = true;

    // 클리어 후 다음 레벨로 이동하기까지의 대기 시간(초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear", meta = (ClampMin = "0.0", EditCondition = "!bQuitGameOnClear"))
    float TravelDelay = 1.0f;

    // 마지막 스테이지(보스)면 켠다. 클리어를 게임 완료로 저장하고, NextLevel(메인 메뉴 등)을 이어하기 위치로 저장하지 않는다.
    // 이동 전에 게임 종료 때와 같은 카운트다운 UI(TravelMessageFormat)를 보여 준다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear")
    bool bFinalStage = false;

    // 마지막 스테이지 카운트다운 안내 메시지. {0}에 QuitCountdownSeconds가 들어간다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear", meta = (EditCondition = "bFinalStage && !bQuitGameOnClear"))
    FText TravelMessageFormat = NSLOCTEXT("LevelClearTrigger", "TravelMessage", "목표에 도달했습니다. {0}초 후 메인 메뉴로 돌아갑니다.");

    // 켜면 레벨 이동 대신 카운트다운 후 게임을 종료한다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear|Quit")
    bool bQuitGameOnClear = false;

    // 게임 종료까지 세는 초
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear|Quit", meta = (ClampMin = "1", EditCondition = "bQuitGameOnClear || bFinalStage"))
    int32 QuitCountdownSeconds = 5;

    // 카운트다운 중 게임을 일시정지할지 여부
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear|Quit", meta = (EditCondition = "bQuitGameOnClear || bFinalStage"))
    bool bPauseGameOnQuitCountdown = true;

    // 카운트다운 안내 메시지. {0}에 QuitCountdownSeconds가 들어간다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LevelClear|Quit", meta = (EditCondition = "bQuitGameOnClear"))
    FText QuitMessageFormat = NSLOCTEXT("LevelClearTrigger", "QuitMessage", "목표에 도달했습니다. {0}초 후 게임이 종료됩니다.");

private:
    bool bIsCleared = false;

    // 카운트다운이 끝나면 게임을 종료하지 않고 NextLevel로 이동한다(마지막 스테이지)
    bool bCountdownTravels = false;

    // 보스 처치 순간의 플레이어 방향을 기록했는지, 그때 -X(왼쪽)를 보고 있었는지
    bool bHasDefeatFacing = false;
    bool bDefeatFacingLeft = false;

    FTimerHandle TravelTimerHandle;

    // 카운트다운에 남은 초
    int32 RemainingQuitSeconds = 0;

    // 일시정지 중에도 돌아가도록 월드 타이머 대신 코어 티커를 사용
    FTSTicker::FDelegateHandle QuitTickerHandle;

    UPROPERTY(Transient)
    TObjectPtr<UGameEndCountdownWidget> CountdownWidget = nullptr;

};
