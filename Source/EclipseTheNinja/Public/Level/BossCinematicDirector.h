// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossCinematicDirector.generated.h"

class ABossEnemy;
class ABossGimmickPortal;
class ATutorialSequenceTrigger;
class UAnimSequenceBase;

/**
 * 보스 스테이지 컷신 연결 액터. 보스 맵에 하나 배치해 보스의 이벤트에 맞춰 컷신 트리거를 재생한다.
 *  - 스테이지 진입 : 레벨 시작 시 IntroSequence를 재생한다. 컷신에는 연출용 보스가 나오므로 실제 보스는 숨겨 둔다
 *  - 에너지 링크 : 링크가 켜지면 같은 순번의 LinkHintSequences를 재생해 레이저가 이어진 길과 기믹 입구를 보여준다
 *  - 처형 : 처형 어빌리티가 시작되면 ExecutionSequence를 두 캐릭터 사이 위치 기준으로 재생한다 (카메라만 있는 시퀀스)
 *  - 그로기 : 링크를 끊고 보스 스테이지 포탈로 돌아오면 보스에게 GroggyAnimation을 재생하고 GroggySequence를 보스 위치 기준으로 재생한다
 * 처형을 뺀 컷신이 재생되는 동안 보스 AI를 멈춘다. 처형은 처형 어빌리티가 직접 멈춘다.
 */
UCLASS()
class ECLIPSETHENINJA_API ABossCinematicDirector : public AActor
{
    GENERATED_BODY()

public:
    ABossCinematicDirector();

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandleEnergyLinkActivated(int32 LinkIndex);

    UFUNCTION()
    void HandleEnergyLinkBroken();

    UFUNCTION()
    void HandleExecutionStarted();

    UFUNCTION()
    void HandlePortalArrived(ABossGimmickPortal* Portal, APawn* Player);

    UFUNCTION()
    void HandleSequenceEnded(ATutorialSequenceTrigger* Trigger);

    // 컷신 동안 보스 AI와 이동을 멈추고, 끝나면 되돌린다
    void PauseBoss();
    void ResumeBoss();

    // 보스 캡슐 바닥 위치. 그로기 컷신의 원점으로 쓴다
    FTransform GetBossFloorTransform() const;

protected:
    // 비워 두면 레벨 시작 시 맵에 있는 보스를 찾아 쓴다
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss Cinematic")
    TObjectPtr<ABossEnemy> Boss = nullptr;

    // 레벨 시작 시 재생할 스테이지 진입 컷신
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss Cinematic")
    TObjectPtr<ATutorialSequenceTrigger> IntroSequence = nullptr;

    // 에너지 링크 순번(보스의 EnergyLinkThresholds 순번)별 힌트 컷신
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss Cinematic")
    TArray<TObjectPtr<ATutorialSequenceTrigger>> LinkHintSequences;

    // 처형 컷신. 원점 근처에 두 캐릭터 사이를 중심으로 만든 카메라 시퀀스
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss Cinematic")
    TObjectPtr<ATutorialSequenceTrigger> ExecutionSequence = nullptr;

    // 그로기 컷신. 원점 근처에 보스 발밑을 중심으로 만든 카메라 시퀀스
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss Cinematic")
    TObjectPtr<ATutorialSequenceTrigger> GroggySequence = nullptr;

    // 그로기 동안 보스에게 재생할 휘청거림 애니메이션
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss Cinematic|Groggy")
    TObjectPtr<UAnimSequenceBase> GroggyAnimation = nullptr;

    // 휘청거림 애니메이션을 재생할 보스 애님 블루프린트의 슬롯
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss Cinematic|Groggy")
    FName GroggySlot = TEXT("DefaultSlot");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss Cinematic|Groggy", meta = (ClampMin = "0.01"))
    float GroggyPlayRate = 1.0f;

private:
    // 링크를 끊은 뒤 아직 보스 스테이지로 돌아오지 않았다
    bool bGroggyPending = false;

    // 이 액터가 보스 AI를 멈춰 두었다
    bool bPausedBoss = false;

    // 진입 컷신 동안 실제 보스를 숨겨 두었다
    bool bHidBoss = false;
};
