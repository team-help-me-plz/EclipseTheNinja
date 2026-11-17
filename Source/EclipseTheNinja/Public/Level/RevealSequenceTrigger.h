#pragma once

#include "CoreMinimal.h"
#include "Interface/Activatable.h"
#include "Level/TutorialSequenceTrigger.h"
#include "RevealSequenceTrigger.generated.h"

/**
 * 스위치·레버가 작동시킬 때 연출 시퀀스를 재생하고, 시퀀스 도중에 실제 대상(문 등)을 작동시키는 트리거.
 *  - 레버의 TargetActors에 실제 문 대신 이 트리거를 넣고, 문은 RevealTargets에 넣는다
 *  - 처음 작동하면 시퀀스를 재생하고, RevealFrame에 도달하면 RevealTargets를 토글한다 (가는 길을 보여 준 뒤 문이 열리는 모습)
 *  - 건너뛰어 RevealFrame 전에 끝나도 끝날 때 토글하므로 문은 반드시 열린다
 *  - 이미 재생한 뒤(Play Once)에 다시 작동하면 시퀀스 없이 바로 토글한다. 레버를 다시 당기거나 체크포인트로 되돌아온 경우도 원래 레버처럼 동작한다
 *  - 레버의 작동 테두리 표시는 RevealTargets에 그대로 전달한다
 */
UCLASS()
class ECLIPSETHENINJA_API ARevealSequenceTrigger : public ATutorialSequenceTrigger, public IActivatable
{
    GENERATED_BODY()

public:
    ARevealSequenceTrigger();

    virtual void ToggleActivate_Implementation() override;
    virtual void ShowActivationIndicator_Implementation() override;
    virtual void HideActivationIndicator_Implementation() override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 시퀀스가 끝나면(건너뛰기 포함) 아직 작동시키지 않은 대상을 작동시킨다
    UFUNCTION()
    void HandleRevealSequenceEnded(ATutorialSequenceTrigger* Trigger);

    // 재생 위치가 RevealFrame에 도달했는지 확인한다
    void CheckRevealFrame();

    // 대기 중인 작동을 처리한다. 이미 처리했으면 아무것도 하지 않는다
    void Reveal();

    // RevealTargets의 Activate를 토글한다. 트리거를 작동시킨 폰을 대상의 Instigator로 넘긴다
    void ToggleRevealTargets();

    // 시퀀스에서 대상을 작동시킬 프레임(표시 프레임 기준). 카메라가 문을 비추는 시점에 맞춘다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reveal", meta = (ClampMin = "0"))
    int32 RevealFrame = 0;

    // 시퀀스 도중 작동시킬 실제 대상(예: 레버로 여는 문). Activatable을 구현한 액터만 처리한다
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Reveal")
    TArray<TObjectPtr<AActor>> RevealTargets;

private:
    // 시퀀스를 재생 중이고 아직 대상을 작동시키지 않았다
    bool bRevealPending = false;

    // 재생 중에 다시 작동 요청이 오면(시간 스위치 복귀 등) 대상 작동 뒤에 이어서 토글할 횟수
    int32 QueuedToggles = 0;

    FTimerHandle RevealTimer;
};
