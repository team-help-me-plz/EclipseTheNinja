// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "TutorialSequenceTrigger.generated.h"

class UArrowComponent;
class ACheckpoint;
class UBoxComponent;
class ALevelSequenceActor;
class ULevelSequence;
class UUserWidget;
class UWidgetComponent;
class UInputAction;
class ACloneCharacter;
class UMaterialInterface;
class USkeletalMeshComponent;
class UInputMappingContext;
class USkipPromptWidget;
struct FInputActionInstance;

/**
 * 튜토리얼 컷신 재생 트리거 액터.
 *  - SequenceTrigger : 플레이어가 닿으면 SequenceActor의 레벨 시퀀스를 재생한다
 *  - bPlayOnBeginPlay를 켜면 트리거 대신 레벨 시작 시 바로 재생한다 (게임 시작 컷신)
 *  - 재생 중에는 플레이어 폰의 입력을 막고 화면 위젯을 숨겼다가, 끝나면 되돌려 조작을 넘긴다
 *  - PlayerEndPoint를 지정하면 재생 시작 시 플레이어를 그 위치로 옮긴다 (연출용 닌자가 끝나는 자리)
 *  - 플레이어 숨김, 카메라 복귀 블렌드는 SequenceActor의 재생 설정과 카메라 컷 트랙이 맡는다
 *  - QTEFrame을 지정하면 그 프레임에서 시퀀스를 거의 멈추고 QTEPrompt 키 안내를 띄운 뒤,
 *    QTEAction 입력이 들어오면 이어서 재생한다 (예: 암살 직전에 암살 키를 직접 누르게 함)
 *  - ActivateFrame에 도달하면 ActivateTargets의 Activate를 토글한다 (예: 분신이 발판을 밟는 순간 실제 문을 연다)
 *    문처럼 Tick으로 메시를 직접 움직이는 액터는 시퀀서 트랜스폼 키가 덮어써지므로 이 방식으로 움직인다
 *  - HandoffCloneFrame에 도달하면 HandoffClonePoint에 실제 분신을 소환한다. 같은 프레임에 시퀀스의 연출용 분신을 없애면
 *    연출 분신이 실제 분신으로 바뀌어, 시퀀스가 끝난 뒤에도 수명 동안 남아 발판을 누르고 있는다
 *  - bTriggerOnOverlap을 끄면 트리거로는 재생하지 않고, 다른 액터가 PlaySequence/PlaySequenceAt을 불러 재생한다 (보스 이벤트 컷신)
 *  - bOriginFollowsMarker를 켜면 SequenceOrigin 마커 위치가 시퀀스 키 공간의 SequenceKeyAnchor 지점이 되도록
 *    TransformOrigin을 자동으로 맞춘다. 컷신 위치를 바꿀 때는 마커(또는 트리거 액터 전체)만 옮기면 된다
 *  - bAllowSkip을 켜면 이미 끝까지 본 시퀀스를 다시 재생할 때 SkipAction 키(IMC_SkipCinematic: ESC·게임패드 메뉴 버튼)를 SkipHoldTime초 동안 눌러 건너뛸 수 있다.
 *    누르는 동안 화면 우측 상단에 테두리가 시계방향으로 차는 안내가 뜨고, 그동안 일시정지 메뉴는 열리지 않는다
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTutorialSequenceEnded, ATutorialSequenceTrigger*, Trigger);

UCLASS()
class ECLIPSETHENINJA_API ATutorialSequenceTrigger : public AActor
{
    GENERATED_BODY()

public:
    ATutorialSequenceTrigger();

    // SequenceActor의 시퀀스를 처음부터 재생하는 함수
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void PlaySequence(APlayerController* PlayerController);

    // 시퀀스의 절대 위치 트랜스폼(스포너블 카메라·연출 메시)을 Origin 기준으로 옮겨 재생하는 함수
    // 원점 근처에 만든 시퀀스를 처형·그로기처럼 매번 위치가 다른 곳에서 재생할 때 쓴다
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void PlaySequenceAt(APlayerController* PlayerController, const FTransform& Origin);

    // 지금 재생 중인지
    UFUNCTION(BlueprintPure, Category = "Tutorial")
    bool IsPlaying() const;

    // 재생 전에 시퀀스 액터가 재생할 시퀀스 에셋을 바꾼다 (예: 플레이어가 반대쪽을 볼 때 좌우 대칭 엔딩)
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void SetSequenceAsset(ULevelSequence* InSequence);

    // 시퀀스 액터에 지금 지정된 시퀀스 에셋
    UFUNCTION(BlueprintPure, Category = "Tutorial")
    ULevelSequence* GetSequenceAsset() const;

    // 시퀀스가 끝난 뒤 맵 이름을 띄우는 입장 시퀀스 트리거인지
    bool ShowsMapNameOnEnd() const { return this->bShowMapNameOnEnd; }

    // 시퀀스가 끝나 조작을 돌려준 직후 호출된다
    UPROPERTY(BlueprintAssignable, Category = "Tutorial")
    FOnTutorialSequenceEnded OnSequenceEnded;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 에디터에서 트리거를 옮기면 시퀀스 TransformOrigin도 바로 따라가게 한다
    virtual void OnConstruction(const FTransform& Transform) override;

    // bOriginFollowsMarker일 때 마커 위치로 SequenceActor의 TransformOrigin을 맞추는 함수
    void ApplyMarkerOrigin();

    UFUNCTION()
    void OnTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    // 시퀀스가 끝나면 막아 둔 입력을 되돌리는 함수
    UFUNCTION()
    void OnSequenceFinished();

    // bHoldLastFrame일 때 재생 설정의 Pause At End로 끝 프레임에 멈추면 호출된다
    UFUNCTION()
    void OnSequencePaused();

    // QTEFrame에 도달하면 재생을 늦추고 키 안내를 띄우는 함수
    void BeginQTE();

    // QTE 키 입력이 들어오면 안내를 닫고 원래 속도로 재생하는 함수
    void OnQTEInput();

    // 재생 위치가 ActivateFrame에 도달했으면 ActivateTargets를 토글하고, 아니면 남은 시간만큼 다시 기다리는 함수
    // QTE로 멈춘 시간이 있어도 실제 재생 위치를 기준으로 맞춘다
    void CheckActivateFrame();
    void CheckAssassinationStabSoundFrame();
    void CheckAssassinationWithdrawalSoundFrame();

    // 화면에 떠 있는 UMG 위젯(체력바·스킬 아이콘 등)을 숨기고 원래 표시 상태를 기억하는 함수. 이미 숨긴 위젯은 건너뛴다
    void HideScreenWidgets();

    // 재생 위치가 HandoffCloneFrame에 도달했으면 실제 분신을 소환하고, 아니면 남은 시간만큼 다시 기다리는 함수
    void CheckHandoffCloneFrame();

    // 재생 위치가 Frame에 도달했으면 true. 아니면 남은 시간 뒤에 Callback을 다시 부르도록 Handle을 걸고 false
    bool WaitUntilFrame(int32 Frame, FTimerHandle& Handle, void (ATutorialSequenceTrigger::*Callback)());

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> SequenceTrigger = nullptr;

    // QTE 키 안내. HUD는 재생 중 숨기므로 화면 공간 위젯 컴포넌트로 따로 띄운다
    // 위젯 클래스는 인스턴스에서 지정 (예: WBP_InteractKeyWidget)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UWidgetComponent> QTEPrompt = nullptr;

    // 컷신 기준 마커. bOriginFollowsMarker일 때 이 위치가 시퀀스 키 공간의 SequenceKeyAnchor 지점이 된다
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UArrowComponent> SequenceOrigin = nullptr;

    // 재생할 레벨 시퀀스 액터. 레벨에 배치한 인스턴스에서 지정
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial")
    TObjectPtr<ALevelSequenceActor> SequenceActor = nullptr;

    // 켜면 SequenceOrigin 마커 위치에서 SequenceKeyAnchor를 뺀 값을 SequenceActor의 TransformOrigin으로 쓴다
    // PlaySequenceAt으로 원점을 직접 넘긴 재생에는 적용하지 않는다
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|Origin")
    bool bOriginFollowsMarker = false;

    // 마커가 나타내는 시퀀스 키 공간의 위치 (예: 연출용 닌자의 위치 키)
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|Origin", meta = (EditCondition = "bOriginFollowsMarker"))
    FVector SequenceKeyAnchor = FVector::ZeroVector;

    // 재생 시작 시 플레이어를 옮길 위치. 비워 두면 옮기지 않는다
    // 시작 시점에 옮겨야 마지막 컷의 블렌드 아웃이 옮겨진 플레이어 카메라로 이어진다
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial")
    TObjectPtr<AActor> PlayerEndPoint = nullptr;

    // 켜면 트리거 대신 레벨 시작 시 재생한다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial")
    bool bPlayOnBeginPlay = false;

    // 인트로가 끝난 뒤 등록하고 활성화할 시작 체크포인트.
    UPROPERTY(EditInstanceOnly, Category = "Tutorial")
    TObjectPtr<ACheckpoint> CheckpointOnFinish = nullptr;

    // 켜면 시퀀스가 끝나 화면 위젯을 되돌린 뒤 맵 이름 표시를 요청한다 (입장 시퀀스 트리거에서만 켠다)
    // 레벨에 이 값이 켜진 트리거가 있으면 입장 즉시 띄우지 않고 이 시퀀스가 끝날 때까지 기다린다. 맵 이름은 레벨당 한 번만 뜬다
    UPROPERTY(EditInstanceOnly, Category = "Tutorial")
    bool bShowMapNameOnEnd = false;

    // 켜면 한 번 재생한 뒤에는 다시 들어와도 재생하지 않는다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial")
    bool bPlayOnce = true;

    // 끄면 플레이어가 트리거에 닿아도 재생하지 않는다. 다른 액터가 PlaySequence를 불러 재생할 때 끈다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial")
    bool bTriggerOnOverlap = true;

    // 켜면 끝 프레임의 카메라·연출을 그대로 둔 채 OnSequenceEnded만 알린다. 입력 차단과 화면 위젯 숨김도 유지한다 (엔딩 마지막 컷 위에 결과를 띄울 때)
    // SequenceActor의 재생 설정에서 Pause At End를 함께 켠다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial")
    bool bHoldLastFrame = false;

    // 0보다 크면 재생 시작 시 화면을 검정에서 이 시간(초) 동안 밝힌다 (게임 시작 컷신의 페이드인)
    // 시퀀스의 페이드 트랙은 이 프로젝트의 카메라 매니저에 반영되지 않아 카메라 매니저 페이드를 직접 건다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial", meta = (ClampMin = "0.0"))
    float StartFadeInDuration = 0.0f;

    // QTE를 걸 시퀀스 프레임(표시 프레임 기준). 0보다 작으면 QTE를 쓰지 않는다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|QTE")
    int32 QTEFrame = -1;

    // QTE에서 기다릴 입력. 게임에서 같은 행동에 쓰는 액션을 지정해 실제 조작 키를 익히게 한다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|QTE")
    TObjectPtr<UInputAction> QTEAction = nullptr;

    UPROPERTY(EditInstanceOnly, Category = "Tutorial|QTE")
    bool bShowQTEInputGlyph = false;

    // QTE에 들어선 직후 잠깐 재생할 속도. 이 속도로 QTEFreezeDelay초 흐른 뒤 완전히 멈춰 입력을 기다린다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|QTE", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float QTESlowPlayRate = 0.15f;

    // 느려진 뒤 완전히 멈추기까지의 시간(초). 0이면 바로 멈춘다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|QTE", meta = (ClampMin = "0.0"))
    float QTEFreezeDelay = 0.3f;

    // 키 안내를 띄울 위치. 비워 두면 QTEPrompt 컴포넌트의 배치 위치를 쓴다
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|QTE")
    TObjectPtr<AActor> QTEPromptAnchor = nullptr;

    // 키 안내 위젯에 표시할 키 글자. 비워 두면 위젯에 적힌 글자를 그대로 쓴다 (예: 분신 QTE는 "R")
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|QTE")
    FText QTEKeyText;

    // QTE 동안 게임의 암살 대상처럼 테두리를 씌울 액터. 이 액터의 첫 스켈레탈 메시에 테두리를 만든다 (예: 연출용 희생 적)
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|QTE")
    TObjectPtr<AActor> QTEOutlineTarget = nullptr;

    // 테두리 머티리얼. 적 블루프린트의 AssassinateTargetComponent와 같은 머티리얼을 지정한다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|QTE")
    TObjectPtr<UMaterialInterface> QTEOutlineMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|QTE", meta = (ClampMin = "0.0", Units = "cm"))
    float QTEOutlineThickness = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|QTE")
    FLinearColor QTEOutlineColor = FLinearColor::Red;

    // Activate를 토글할 시퀀스 프레임(표시 프레임 기준). 0보다 작으면 쓰지 않는다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Activate")
    int32 ActivateFrame = -1;

    // 암살 어빌리티와 같은 플레이어 사운드를 지정 프레임에서 재생한다.
    UPROPERTY(EditInstanceOnly, Category = "Tutorial|Assassination Sound")
    int32 AssassinationStabSoundFrame = -1;

    UPROPERTY(EditInstanceOnly, Category = "Tutorial|Assassination Sound")
    int32 AssassinationWithdrawalSoundFrame = -1;

    // ActivateFrame에 토글할 액터들. Activatable 인터페이스를 구현한 액터만 처리한다 (예: 문)
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|Activate")
    TArray<TObjectPtr<AActor>> ActivateTargets;

    // 켜면 시퀀스가 끝날 때 다시 토글해 원래 상태로 되돌린다 (연출만 보여 주고 플레이어가 직접 해 보게 할 때)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Activate")
    bool bRevertActivateOnFinish = true;

    // 실제 분신을 소환할 시퀀스 프레임(표시 프레임 기준). 0보다 작으면 쓰지 않는다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Clone")
    int32 HandoffCloneFrame = -1;

    // 소환할 분신 클래스. 분신술 어빌리티와 같은 클래스를 지정한다 (예: BP_CloneCharacterNinja)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Clone")
    TSubclassOf<ACloneCharacter> HandoffCloneClass;

    // 분신을 소환할 위치와 방향. 연출용 분신이 사라지는 자리의 캡슐 중심에 둔다
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|Clone")
    TObjectPtr<AActor> HandoffClonePoint = nullptr;

    // 소환한 분신의 수명(초). 수명이 끝나면 분신술로 만든 분신처럼 사라진다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Clone", meta = (ClampMin = "0.01"))
    float HandoffCloneLifeTime = 5.0f;

    // 켜면 실제 분신을 소환할 때 플레이어 분신술의 쿨타임도 시작한다 (직접 쓴 것과 같게)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Clone")
    bool bStartCloneCooldownOnHandoff = true;

    // 켜면 이미 끝까지 본 시퀀스를 다시 재생할 때 SkipAction 키를 길게 눌러 건너뛸 수 있다 (예: 보스 진입 컷신)
    // 건너뛰면 끝 프레임으로 바로 넘어가므로 QTE·ActivateFrame·HandoffCloneFrame 처리는 하지 않는다. 연출만 있는 시퀀스에 켠다
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|Skip")
    bool bAllowSkip = false;

    // 건너뛰기 입력 액션. 기본은 IA_SkipCinematic
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Skip", meta = (EditCondition = "bAllowSkip"))
    TObjectPtr<UInputAction> SkipAction = nullptr;

    // 건너뛰기 키를 정하는 매핑 컨텍스트. 기본은 IMC_SkipCinematic(ESC·게임패드 메뉴 버튼, 일시정지와 같은 키)
    // 건너뛰기를 받는 동안 게임플레이 IMC보다 높은 우선순위로 넣고, 일시정지 메뉴는 열리지 않게 막는다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Skip", meta = (EditCondition = "bAllowSkip"))
    TObjectPtr<UInputMappingContext> SkipMappingContext = nullptr;

    // 이 시간(초) 동안 계속 누르면 건너뛴다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Skip", meta = (EditCondition = "bAllowSkip", ClampMin = "0.05", Units = "s"))
    float SkipHoldTime = 0.5f;

    // 건너뛴 직후 화면을 검정에서 이 시간(초) 동안 밝혀 컷이 툭 끊기지 않게 한다. 0이면 페이드 없이 넘어간다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Skip", meta = (EditCondition = "bAllowSkip", ClampMin = "0.0", Units = "s"))
    float SkipFadeInDuration = 0.3f;

    // 누르는 동안 우측 상단에 띄울 안내 위젯. 모양을 바꾸려면 USkipPromptWidget을 상속한 위젯 블루프린트를 지정한다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial|Skip", meta = (EditCondition = "bAllowSkip"))
    TSubclassOf<USkipPromptWidget> SkipPromptClass;

private:
    // 지금 재생이 건너뛰기를 받을 수 있으면 건너뛰기 입력을 켜고 일시정지 메뉴를 막는다
    void BeginSkipInput();

    // 건너뛰기 입력을 끄고 안내를 지우고 일시정지 메뉴 차단을 푼다. 켜져 있지 않으면 아무것도 하지 않는다
    void EndSkipInput();

    // QTE·건너뛰기 중 지금 필요한 입력만 이 액터의 입력 컴포넌트에 다시 바인딩한다. 둘 다 필요 없으면 입력을 끈다
    void RefreshTriggerInput();

    // 건너뛰기 키를 누르고 있는 동안 매 프레임 호출된다. 누른 시간이 SkipHoldTime에 닿으면 건너뛴다
    void OnSkipHeld(const FInputActionInstance& Instance);

    // SkipHoldTime 전에 키를 떼면 안내를 숨기고 진행을 되돌린다
    void OnSkipReleased();

    // 끝 프레임으로 넘어가 시퀀스가 정상 종료 경로로 끝나게 한다
    void SkipSequence();

    // 본 기록에 쓰는 시퀀스 에셋의 패키지 이름. 시퀀스가 없으면 NAME_None
    FName GetSeenSequenceKey() const;

    // bAllowSkip일 때 지금 시퀀스를 끝까지 본 것으로 기록한다
    void MarkSequenceSeen() const;

    bool bSkipEnabled = false;
    bool bSkipped = false;

    UPROPERTY(Transient)
    TObjectPtr<USkipPromptWidget> SkipPrompt = nullptr;

    bool bHasPlayed = false;

    // PlaySequenceAt이 원점을 직접 정한 재생이면 마커 원점을 덮어쓰지 않는다
    bool bOriginOverridden = false;

    // 입력을 막아 둔 폰과 컨트롤러. 시퀀스가 끝나면 되돌린다
    TWeakObjectPtr<APawn> BlockedPawn;
    TWeakObjectPtr<APlayerController> BlockedController;

    // 재생 중 숨겨 둔 화면 위젯과 원래 Visibility. 시퀀스가 끝나면 되돌린다
    TMap<TWeakObjectPtr<UUserWidget>, ESlateVisibility> HiddenWidgets;

    // 게임 시작 컷신은 HUD 위젯이 만들어지기 전에 재생되므로, 재생 직후 잠깐 동안 다시 숨기기를 반복하는 타이머
    FTimerHandle WidgetHideTimer;
    int32 WidgetHideRetries = 0;

    FTimerHandle QTETimer;
    FTimerHandle QTEFreezeTimer;
    bool bWaitingForQTE = false;

    FTimerHandle ActivateTimer;
    FTimerHandle AssassinationStabSoundTimer;
    FTimerHandle AssassinationWithdrawalSoundTimer;
    // 이번 재생에서 토글한 액터. bRevertActivateOnFinish일 때 끝나면 다시 토글한다
    TArray<TWeakObjectPtr<AActor>> ActivatedActors;

    FTimerHandle HandoffCloneTimer;

    // QTEOutlineTarget에 만든 테두리 메시. 처음 QTE에 들어갈 때 만들고 이후 보이기만 바꾼다
    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> QTEOutlineMesh = nullptr;

    void SetQTEOutlineVisible(bool bVisible);

};
