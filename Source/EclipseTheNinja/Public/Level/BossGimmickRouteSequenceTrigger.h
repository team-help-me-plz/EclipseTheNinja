#pragma once

#include "CoreMinimal.h"
#include "Level/TutorialSequenceTrigger.h"
#include "BossGimmickRouteSequenceTrigger.generated.h"

class ABossEnemy;
class ABossGimmickPortal;

/**
 * 보스 기믹 방 루트 미리보기 컷신 트리거.
 *  - EntryPortal(기믹 방 시작 포탈)로 플레이어가 들어오면 시퀀스를 재생해 레버까지 가는 길을 훑어 보여준다
 *  - 재생하는 동안 보스의 에너지 링크 제한시간을 멈추고, 끝나거나 건너뛰면 남은 시간부터 다시 흐르게 한다
 *  - bAllowSkipFirstTime이 켜져 있으면 처음 보는 재생도 건너뛸 수 있다 (부모의 건너뛰기는 이미 본 시퀀스에만 열린다)
 *  - 트리거 상자로는 재생하지 않는다. 기믹 방 레벨에 배치하고 EntryPortal과 Sequence Actor만 지정하면 된다
 */
UCLASS()
class ECLIPSETHENINJA_API ABossGimmickRouteSequenceTrigger : public ATutorialSequenceTrigger
{
    GENERATED_BODY()

public:
    ABossGimmickRouteSequenceTrigger();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // EntryPortal에 플레이어가 도착하면(화면이 다시 밝아지기 시작할 때) 재생한다
    UFUNCTION()
    void HandlePortalArrived(ABossGimmickPortal* Portal, APawn* Player);

    // 재생이 끝나면(건너뛰기 포함) 멈춰 둔 링크 제한시간을 다시 흐르게 한다
    UFUNCTION()
    void HandleRouteSequenceEnded(ATutorialSequenceTrigger* Trigger);

    // 기믹 방 시작 포탈(Portal Side = Gimmick Stage). 이 포탈로 들어올 때 재생하며, 이 포탈의 LinkId로 보스를 찾는다
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss Gimmick Route")
    TObjectPtr<ABossGimmickPortal> EntryPortal = nullptr;

    // 켜면 처음 보는 재생도 건너뛸 수 있다. Allow Skip이 켜져 있어야 쓰인다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss Gimmick Route")
    bool bAllowSkipFirstTime = true;

private:
    // EntryPortal의 링크를 가진 보스. 기믹 스테이지는 보스 맵에 레벨 인스턴스로 들어가므로 같은 월드에서 찾는다
    ABossEnemy* FindLinkedBoss() const;

    void PauseLinkTimer();
    void ResumeLinkTimer();

    // 링크 제한시간을 멈춘 보스. 멈추지 않았으면 비어 있다
    TWeakObjectPtr<ABossEnemy> PausedBoss;
};
