// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/TutorialSequenceTrigger.h"

#include "Character/PlayerCharacter.h"
#include "Character/PlayerCharacterController.h"
#include "Checkpoint/Checkpoint.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "Clone/CloneCharacter.h"
#include "Component/AssassinateTargetComponent.h"
#include "GAS/Ability/GACloneTech.h"
#include "ObjectPool/ObjectPoolSubsystem.h"
#include "Save/GameSaveSubsystem.h"
#include "Subsystem/MapNameSubsystem.h"
#include "Widgets/HUDWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

#include "Blueprint/UserWidget.h"
#include "Widgets/InputPromptWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ArrowComponent.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Interface/Activatable.h"
#include "Kismet/GameplayStatics.h"
#include "DefaultLevelSequenceInstanceData.h"
#include "LevelSequenceActor.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ATutorialSequenceTrigger::ATutorialSequenceTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    this->SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(this->SceneRoot);

    this->SequenceTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("SequenceTrigger"));
    this->SequenceTrigger->SetupAttachment(this->SceneRoot);
    this->SequenceTrigger->SetBoxExtent(FVector(50.0f, 200.0f, 150.0f));
    this->SequenceTrigger->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
    this->SequenceTrigger->SetCollisionProfileName(TEXT("Trigger"));
    this->SequenceTrigger->ShapeColor = FColor::Magenta;

    this->QTEPrompt = CreateDefaultSubobject<UWidgetComponent>(TEXT("QTEPrompt"));
    this->QTEPrompt->SetupAttachment(this->SceneRoot);
    this->QTEPrompt->SetWidgetSpace(EWidgetSpace::Screen);
    this->QTEPrompt->SetDrawAtDesiredSize(true);
    this->QTEPrompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    this->QTEPrompt->SetHiddenInGame(true);

    this->SequenceOrigin = CreateDefaultSubobject<UArrowComponent>(TEXT("SequenceOrigin"));
    this->SequenceOrigin->SetupAttachment(this->SceneRoot);
    this->SequenceOrigin->ArrowColor = FColor::Cyan;
    this->SequenceOrigin->ArrowSize = 2.0f;
    this->SequenceOrigin->SetHiddenInGame(true);
}

void ATutorialSequenceTrigger::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyMarkerOrigin();
}

void ATutorialSequenceTrigger::ApplyMarkerOrigin()
{
    if (!this->bOriginFollowsMarker || !IsValid(this->SequenceActor))
    {
        return;
    }

    UDefaultLevelSequenceInstanceData* InstanceData = Cast<UDefaultLevelSequenceInstanceData>(this->SequenceActor->DefaultInstanceData);
    if (!InstanceData)
    {
        return;
    }

    const FVector Origin = this->SequenceOrigin->GetComponentLocation() - this->SequenceKeyAnchor;
    if (this->SequenceActor->bOverrideInstanceData && InstanceData->TransformOrigin.GetLocation().Equals(Origin, 0.01f)
        && InstanceData->TransformOrigin.GetRotation().IsIdentity())
    {
        return;
    }

    // 값이 바뀔 때만 수정 표시를 남겨, 트리거를 옮기지 않았는데 시퀀스 액터가 더러워지지 않게 한다
    this->SequenceActor->Modify();
    InstanceData->Modify();
    this->SequenceActor->bOverrideInstanceData = true;
    InstanceData->TransformOrigin = FTransform(Origin);
}

void ATutorialSequenceTrigger::BeginPlay()
{
    Super::BeginPlay();

    // 벽 붙기 트레이스(WallAttach)가 트리거 상자에 막히지 않게 한다. Trigger 프로필에는 WallAttach 채널이 없어 기본값 Block이 적용되므로 런타임에 지정한다.
    this->SequenceTrigger->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);

    if (this->bPlayOnBeginPlay)
    {
        // 시작 컷신은 오버랩이 필요 없으므로 트리거를 끈다
        this->SequenceTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PlaySequence(UGameplayStatics::GetPlayerController(this, 0));
        return;
    }

    if (!this->bTriggerOnOverlap)
    {
        this->SequenceTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        return;
    }

    this->SequenceTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATutorialSequenceTrigger::OnTriggerBeginOverlap);
}

void ATutorialSequenceTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 재생 도중 맵을 떠나도 일시정지 메뉴 차단과 건너뛰기 IMC가 남지 않게 한다
    EndSkipInput();
    Super::EndPlay(EndPlayReason);
}

void ATutorialSequenceTrigger::PlaySequenceAt(APlayerController* PlayerController, const FTransform& Origin)
{
    if (!IsValid(this->SequenceActor))
    {
        return;
    }

    // 시퀀스의 절대 트랜스폼 트랙이 Origin 기준으로 평가되도록 인스턴스 데이터를 덮어쓴다
    this->SequenceActor->bOverrideInstanceData = true;
    if (UDefaultLevelSequenceInstanceData* InstanceData = Cast<UDefaultLevelSequenceInstanceData>(this->SequenceActor->DefaultInstanceData))
    {
        InstanceData->TransformOrigin = Origin;
    }

    this->bOriginOverridden = true;
    PlaySequence(PlayerController);
    this->bOriginOverridden = false;
}

void ATutorialSequenceTrigger::SetSequenceAsset(ULevelSequence* InSequence)
{
    if (!IsValid(this->SequenceActor) || !IsValid(InSequence) || IsPlaying())
    {
        return;
    }
    if (this->SequenceActor->GetSequence() != InSequence)
    {
        this->SequenceActor->SetSequence(InSequence);
    }
}

ULevelSequence* ATutorialSequenceTrigger::GetSequenceAsset() const
{
    return IsValid(this->SequenceActor) ? Cast<ULevelSequence>(this->SequenceActor->GetSequence()) : nullptr;
}

bool ATutorialSequenceTrigger::IsPlaying() const
{
    const ULevelSequencePlayer* SequencePlayer = IsValid(this->SequenceActor) ? this->SequenceActor->GetSequencePlayer() : nullptr;
    return SequencePlayer && SequencePlayer->IsPlaying();
}

void ATutorialSequenceTrigger::OnTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    // 분신 등 AI 폰은 무시하고 플레이어가 조종하는 폰만 처리
    const APawn* Pawn = Cast<APawn>(OtherActor);
    if (!IsValid(Pawn) || !Pawn->IsPlayerControlled())
    {
        return;
    }

    PlaySequence(Cast<APlayerController>(Pawn->GetController()));
}

void ATutorialSequenceTrigger::PlaySequence(APlayerController* PlayerController)
{
    if (!IsValid(this->SequenceActor) || (this->bPlayOnce && this->bHasPlayed))
    {
        return;
    }

    ULevelSequencePlayer* SequencePlayer = this->SequenceActor->GetSequencePlayer();
    if (!SequencePlayer)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ATutorialSequenceTrigger::PlaySequence()] %s : 시퀀스 플레이어 없음"), *GetActorNameOrLabel());
        return;
    }
    this->bHasPlayed = true;

    if (!this->bOriginOverridden)
    {
        ApplyMarkerOrigin();
    }

    // 재생 설정의 이동 입력 차단만으로는 공격·대시 같은 어빌리티 입력이 남으므로 폰 입력을 통째로 막는다
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (IsValid(PlayerPawn))
    {
        PlayerPawn->DisableInput(PlayerController);
        this->BlockedPawn = PlayerPawn;
        this->BlockedController = PlayerController;

        // 앉기는 키를 누르는 동안만 유지되는데, 입력을 막으면 키를 뗀 입력이 사라져 시퀀스가 끝난 뒤에도 앉은 채로 남는다.
        // 컷신 동안 플레이어는 숨겨지므로 시작할 때 일으켜 세우고, 남은 이동 속도도 없앤다
        if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(PlayerPawn))
        {
            PlayerCharacter->DoUnCrouch();
            PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();
        }

        if (IsValid(this->PlayerEndPoint))
        {
            PlayerPawn->TeleportTo(this->PlayerEndPoint->GetActorLocation(), PlayerPawn->GetActorRotation());
        }
    }

    // 건너뛸 수 있는 재생이면 화면 위젯을 숨기기 전에 HUD의 건너뛰기 안내를 찾아, HUD 안에서 안내만 남기고 숨기게 한다
    this->bSkipped = false;
    BeginSkipInput();

    // 재생 설정의 HUD 숨김은 AHUD만 숨기므로 체력바·스킬 아이콘 같은 UMG 위젯은 직접 숨긴다
    HideScreenWidgets();
    // 레벨 시작과 함께 재생하면 HUD 위젯이 아직 만들어지기 전이므로, 0.1초 간격으로 몇 번 더 숨긴다
    this->WidgetHideRetries = 10;
    GetWorldTimerManager().SetTimer(this->WidgetHideTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        HideScreenWidgets();
        if (--this->WidgetHideRetries <= 0 || !IsPlaying())
        {
            GetWorldTimerManager().ClearTimer(this->WidgetHideTimer);
        }
    }), 0.1f, true);

    SequencePlayer->OnFinished.AddUniqueDynamic(this, &ATutorialSequenceTrigger::OnSequenceFinished);
    if (this->bHoldLastFrame)
    {
        // Pause At End로 끝 프레임에 멈추면 OnFinished 대신 OnPause가 온다
        SequencePlayer->OnPause.AddUniqueDynamic(this, &ATutorialSequenceTrigger::OnSequencePaused);
    }
    SequencePlayer->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(0.0f, EUpdatePositionMethod::Jump));
    SequencePlayer->SetPlayRate(1.0f);
    SequencePlayer->Play();

    if (this->StartFadeInDuration > 0.0f && IsValid(PlayerController) && IsValid(PlayerController->PlayerCameraManager))
    {
        PlayerController->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, this->StartFadeInDuration, FLinearColor::Black, false, false);
    }

    // 시퀀스는 월드 시간으로 재생되므로 같은 월드 타이머로 QTE 프레임을 기다린다
    if (this->QTEFrame >= 0 && IsValid(this->QTEAction) && IsValid(PlayerController))
    {
        const float QTETime = static_cast<float>(SequencePlayer->GetFrameRate().AsSeconds(FFrameTime(this->QTEFrame)));
        GetWorldTimerManager().SetTimer(this->QTETimer, this, &ATutorialSequenceTrigger::BeginQTE, FMath::Max(QTETime, 0.01f), false);
    }

    if (this->ActivateFrame >= 0 && this->ActivateTargets.Num() > 0)
    {
        this->ActivatedActors.Reset();
        CheckActivateFrame();
    }

    if (this->AssassinationStabSoundFrame >= 0)
    {
        CheckAssassinationStabSoundFrame();
    }
    if (this->AssassinationWithdrawalSoundFrame >= 0)
    {
        CheckAssassinationWithdrawalSoundFrame();
    }

    if (this->HandoffCloneFrame >= 0 && this->HandoffCloneClass && IsValid(this->HandoffClonePoint))
    {
        CheckHandoffCloneFrame();
    }

    UE_LOG(LogTemp, Log, TEXT("[ATutorialSequenceTrigger::PlaySequence()] %s : 시퀀스 재생 시작"), *GetActorNameOrLabel());
}

void ATutorialSequenceTrigger::BeginQTE()
{
    APlayerController* PlayerController = this->BlockedController.Get();
    ULevelSequencePlayer* SequencePlayer = IsValid(this->SequenceActor) ? this->SequenceActor->GetSequencePlayer() : nullptr;
    if (!PlayerController || !SequencePlayer || !SequencePlayer->IsPlaying())
    {
        return;
    }

    // 폰 입력은 막혀 있으므로 이 액터의 입력 컴포넌트를 컨트롤러 입력 스택에 올려 QTE 키만 받는다
    EnableInput(PlayerController);
    if (!Cast<UEnhancedInputComponent>(this->InputComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("[ATutorialSequenceTrigger::BeginQTE()] %s : EnhancedInputComponent 없음, QTE 건너뜀"), *GetActorNameOrLabel());
        RefreshTriggerInput();
        return;
    }

    // 시간이 느려지다 멈추는 느낌을 주고, 기다리는 동안 동작이 저절로 진행되지 않게 완전히 멈춘다
    this->bWaitingForQTE = true;
    RefreshTriggerInput();
    if (this->QTEFreezeDelay > 0.0f)
    {
        SequencePlayer->SetPlayRate(this->QTESlowPlayRate);
        TWeakObjectPtr<ULevelSequencePlayer> WeakPlayer = SequencePlayer;
        GetWorldTimerManager().SetTimer(this->QTEFreezeTimer, FTimerDelegate::CreateWeakLambda(this, [this, WeakPlayer]()
        {
            if (this->bWaitingForQTE && WeakPlayer.IsValid())
            {
                WeakPlayer->Pause();
            }
        }), this->QTEFreezeDelay, false);
    }
    else
    {
        SequencePlayer->Pause();
    }

    if (IsValid(this->QTEPromptAnchor))
    {
        this->QTEPrompt->SetWorldLocation(this->QTEPromptAnchor->GetActorLocation());
    }

    UUserWidget* PromptWidget = this->QTEPrompt->GetUserWidgetObject();
    if (UInputPromptWidget* InputPrompt = Cast<UInputPromptWidget>(PromptWidget))
    {
        if (this->bShowQTEInputGlyph)
        {
            InputPrompt->SetQTEPromptAction(this->QTEAction, this->QTEKeyText);
        }
        else
        {
            InputPrompt->SetPromptAction(this->QTEAction, this->QTEKeyText);
        }
    }
    else if (!this->QTEKeyText.IsEmpty() && IsValid(PromptWidget) && PromptWidget->WidgetTree)
    {
        UTextBlock* KeyTextBlock = nullptr;
        PromptWidget->WidgetTree->ForEachWidget([&KeyTextBlock](UWidget* Widget)
        {
            if (!KeyTextBlock)
            {
                KeyTextBlock = Cast<UTextBlock>(Widget);
            }
        });
        if (KeyTextBlock)
        {
            KeyTextBlock->SetText(this->QTEKeyText);

            // 키 상자는 한 글자 폭으로 고정돼 있다. "좌클릭"처럼 긴 글자는 상자를 글자 폭에 맞춰 늘린다
            if (this->QTEKeyText.ToString().Len() > 1)
            {
                // 배경 이미지는 원본 크기가 커서 고정 폭을 풀면 상자가 화면만큼 넓어진다.
                // 처음 표시 전에는 글자 크기를 잴 수 없으므로 한 글자 폭을 기준으로 글자 수만큼 늘린다
                USizeBox* KeyBox = Cast<USizeBox>(PromptWidget->WidgetTree->RootWidget);
                if (IsValid(KeyBox) && KeyBox->GetWidthOverride() > 0.0f)
                {
                    // 다시 재생할 때 넓힌 폭이 또 늘지 않도록 원래 한 글자 폭을 최소 폭에 기억해 둔다
                    if (KeyBox->GetMinDesiredWidth() <= 0.0f)
                    {
                        KeyBox->SetMinDesiredWidth(KeyBox->GetWidthOverride());
                    }
                    const float BaseWidth = KeyBox->GetMinDesiredWidth();
                    const int32 CharCount = this->QTEKeyText.ToString().Len();
                    KeyBox->SetWidthOverride(BaseWidth * (0.35f + 0.65f * CharCount));
                }
            }
        }
    }
    this->QTEPrompt->SetHiddenInGame(false);
    SetQTEOutlineVisible(true);

    UE_LOG(LogTemp, Log, TEXT("[ATutorialSequenceTrigger::BeginQTE()] %s : QTE 입력 대기"), *GetActorNameOrLabel());
}

void ATutorialSequenceTrigger::OnQTEInput()
{
    if (!this->bWaitingForQTE)
    {
        return;
    }
    this->bWaitingForQTE = false;
    GetWorldTimerManager().ClearTimer(this->QTEFreezeTimer);

    this->QTEPrompt->SetHiddenInGame(true);
    SetQTEOutlineVisible(false);
    // 건너뛰기 입력은 남기고 QTE 바인딩만 뺀다
    RefreshTriggerInput();

    ULevelSequencePlayer* SequencePlayer = IsValid(this->SequenceActor) ? this->SequenceActor->GetSequencePlayer() : nullptr;
    if (SequencePlayer)
    {
        SequencePlayer->SetPlayRate(1.0f);
        if (!SequencePlayer->IsPlaying())
        {
            SequencePlayer->Play();
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[ATutorialSequenceTrigger::OnQTEInput()] %s : QTE 입력, 재생 재개"), *GetActorNameOrLabel());
}

void ATutorialSequenceTrigger::SetQTEOutlineVisible(bool bVisible)
{
    if (!IsValid(this->QTEOutlineTarget))
    {
        return;
    }

    if (!IsValid(this->QTEOutlineMesh) && bVisible)
    {
        // 게임에서 암살 가능한 적에 뜨는 것과 같은 테두리를 만든다
        USkeletalMeshComponent* SourceMesh = this->QTEOutlineTarget->FindComponentByClass<USkeletalMeshComponent>();
        this->QTEOutlineMesh = UAssassinateTargetComponent::CreateOutlineMesh(SourceMesh, this->QTEOutlineMaterial,
            this->QTEOutlineThickness, this->QTEOutlineColor);
    }

    if (IsValid(this->QTEOutlineMesh))
    {
        this->QTEOutlineMesh->SetVisibility(bVisible);
    }
}

bool ATutorialSequenceTrigger::WaitUntilFrame(int32 Frame, FTimerHandle& Handle, void (ATutorialSequenceTrigger::*Callback)())
{
    ULevelSequencePlayer* SequencePlayer = IsValid(this->SequenceActor) ? this->SequenceActor->GetSequencePlayer() : nullptr;
    if (!SequencePlayer)
    {
        return false;
    }

    const int32 CurrentFrame = SequencePlayer->GetCurrentTime().Time.FrameNumber.Value;
    if (CurrentFrame >= Frame)
    {
        return true;
    }

    const float Remaining = static_cast<float>(SequencePlayer->GetFrameRate().AsSeconds(FFrameTime(Frame - CurrentFrame)));
    GetWorldTimerManager().SetTimer(Handle, this, Callback, FMath::Max(Remaining, 0.02f), false);
    return false;
}

void ATutorialSequenceTrigger::CheckActivateFrame()
{
    if (!WaitUntilFrame(this->ActivateFrame, this->ActivateTimer, &ATutorialSequenceTrigger::CheckActivateFrame))
    {
        return;
    }

    for (AActor* Target : this->ActivateTargets)
    {
        if (IsValid(Target) && Target->Implements<UActivatable>())
        {
            IActivatable::Execute_ToggleActivate(Target);
            this->ActivatedActors.Add(Target);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[ATutorialSequenceTrigger::CheckActivateFrame()] %s : 대상 %d개 토글"), *GetActorNameOrLabel(), this->ActivatedActors.Num());
}

void ATutorialSequenceTrigger::CheckAssassinationStabSoundFrame()
{
    if (!WaitUntilFrame(AssassinationStabSoundFrame, AssassinationStabSoundTimer, &ATutorialSequenceTrigger::CheckAssassinationStabSoundFrame)) return;

    if (const APlayerCharacter* Player = Cast<APlayerCharacter>(BlockedPawn.Get()))
    {
        Player->PlayActionSound(EPlayerActionSound::AssassinationStab);
    }
}

void ATutorialSequenceTrigger::CheckAssassinationWithdrawalSoundFrame()
{
    if (!WaitUntilFrame(AssassinationWithdrawalSoundFrame, AssassinationWithdrawalSoundTimer, &ATutorialSequenceTrigger::CheckAssassinationWithdrawalSoundFrame)) return;

    if (const APlayerCharacter* Player = Cast<APlayerCharacter>(BlockedPawn.Get()))
    {
        Player->PlayAssassinationWithdrawalSound();
    }
}

void ATutorialSequenceTrigger::CheckHandoffCloneFrame()
{
    if (!WaitUntilFrame(this->HandoffCloneFrame, this->HandoffCloneTimer, &ATutorialSequenceTrigger::CheckHandoffCloneFrame))
    {
        return;
    }

    APawn* ClonePawnOwner = this->BlockedPawn.Get();
    UObjectPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<UObjectPoolSubsystem>();
    if (!IsValid(ClonePawnOwner) || !PoolSubsystem)
    {
        return;
    }

    // 분신술 어빌리티와 같은 방식으로 소환한다. 연출 분신과 자리가 바뀌는 순간이라 소환 이펙트는 재생하지 않는다
    const FTransform SpawnTransform(this->HandoffClonePoint->GetActorRotation(), this->HandoffClonePoint->GetActorLocation());
    ACloneCharacter* Clone = PoolSubsystem->SpawnActor<ACloneCharacter>(this->HandoffCloneClass, SpawnTransform, ClonePawnOwner, ClonePawnOwner);
    if (!IsValid(Clone))
    {
        UE_LOG(LogTemp, Warning, TEXT("[ATutorialSequenceTrigger::CheckHandoffCloneFrame()] %s : 분신 소환 실패"), *GetActorNameOrLabel());
        return;
    }

    // 제자리로 이동 명령을 줘서 그 자리에 서 있게 한다
    Clone->SetCommand(FCloneCommandType::MakeMove(Clone->GetActorLocation()));
    // 수명이 끝나면 ACloneCharacter::LifeSpanExpired에서 풀에 반환된다
    Clone->SetLifeSpan(this->HandoffCloneLifeTime);

    if (UCheckpointSubsystem* Subsystem = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
    {
        Subsystem->RegisterRuntimeSpawnedActor(Clone);
    }

    // 분신술을 직접 쓴 것과 같게 쿨타임을 시작한다. 쿨타임 UI는 어빌리티 인스턴스의 시작 알림을 듣는다
    if (this->bStartCloneCooldownOnHandoff)
    {
        if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ClonePawnOwner))
        {
            for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
            {
                if (!Spec.Ability || !Spec.Ability->IsA<UGACloneTech>())
                {
                    continue;
                }
                const UGACooldownBase* Ability = Cast<UGACooldownBase>(Spec.GetPrimaryInstance());
                if (!Ability)
                {
                    Ability = Cast<UGACooldownBase>(Spec.Ability);
                }
                Ability->StartCooldownWithoutActivation(Spec.Handle, ASC->AbilityActorInfo.Get());
                break;
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[ATutorialSequenceTrigger::CheckHandoffCloneFrame()] %s : 실제 분신 소환"), *GetActorNameOrLabel());
}

void ATutorialSequenceTrigger::OnSequencePaused()
{
    // QTE 대기로 멈춘 것은 끝이 아니다
    ULevelSequencePlayer* SequencePlayer = IsValid(this->SequenceActor) ? this->SequenceActor->GetSequencePlayer() : nullptr;
    if (this->bWaitingForQTE || !SequencePlayer)
    {
        return;
    }
    if (SequencePlayer->GetCurrentTime().Time.FrameNumber < SequencePlayer->GetEndTime().Time.FrameNumber - 1)
    {
        return;
    }
    SequencePlayer->OnPause.RemoveDynamic(this, &ATutorialSequenceTrigger::OnSequencePaused);

    UE_LOG(LogTemp, Log, TEXT("[ATutorialSequenceTrigger::OnSequencePaused()] %s : 끝 프레임에서 멈춤, 마지막 컷 유지"), *GetActorNameOrLabel());

    // 마지막 컷 위에 결과를 띄우는 동안 ESC는 다시 일시정지 메뉴가 받게 한다
    EndSkipInput();
    // 건너뛰기 안내 때문에 남겨 둔 HUD도 마지막 컷 동안은 숨긴다
    HideScreenWidgets();
    MarkSequenceSeen();

    this->OnSequenceEnded.Broadcast(this);
}

void ATutorialSequenceTrigger::HideScreenWidgets()
{
    TArray<UUserWidget*> TopLevelWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, TopLevelWidgets, UUserWidget::StaticClass(), true);
    for (UUserWidget* Widget : TopLevelWidgets)
    {
        if (!IsValid(Widget) || Widget == this->QTEPrompt->GetUserWidgetObject())
        {
            continue;
        }
        // 건너뛸 수 있는 재생이면 HUD는 숨기지 않는다. HUD가 건너뛰기 안내만 남기고 나머지를 직접 숨긴다
        if (this->bSkipEnabled && Widget->IsA<UHUDWidget>())
        {
            continue;
        }
        if (Widget->IsVisible() && !this->HiddenWidgets.Contains(Widget))
        {
            this->HiddenWidgets.Add(Widget, Widget->GetVisibility());
            Widget->SetVisibility(ESlateVisibility::Hidden);
        }
    }
}

void ATutorialSequenceTrigger::OnSequenceFinished()
{
    // Pause At End로 끝 프레임에 멈출 때도 OnFinished가 함께 온다. 마지막 컷을 유지해야 하므로 되돌리지 않고 OnSequencePaused에 맡긴다
    if (this->bHoldLastFrame)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(this->QTETimer);
    GetWorldTimerManager().ClearTimer(this->QTEFreezeTimer);
    GetWorldTimerManager().ClearTimer(this->ActivateTimer);
    GetWorldTimerManager().ClearTimer(this->AssassinationStabSoundTimer);
    GetWorldTimerManager().ClearTimer(this->AssassinationWithdrawalSoundTimer);
    GetWorldTimerManager().ClearTimer(this->HandoffCloneTimer);
    GetWorldTimerManager().ClearTimer(this->WidgetHideTimer);

    if (this->bRevertActivateOnFinish)
    {
        for (const TWeakObjectPtr<AActor>& Target : this->ActivatedActors)
        {
            if (Target.IsValid())
            {
                IActivatable::Execute_ToggleActivate(Target.Get());
            }
        }
    }
    this->ActivatedActors.Reset();

    if (this->bWaitingForQTE)
    {
        this->bWaitingForQTE = false;
        this->QTEPrompt->SetHiddenInGame(true);
        SetQTEOutlineVisible(false);
        DisableInput(this->BlockedController.Get());
    }

    // BlockedController를 비우기 전에 건너뛰기 입력을 정리한다
    EndSkipInput();
    MarkSequenceSeen();

    if (this->BlockedPawn.IsValid())
    {
        this->BlockedPawn->EnableInput(this->BlockedController.Get());
    }
    this->BlockedPawn.Reset();
    this->BlockedController.Reset();

    for (const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Pair : this->HiddenWidgets)
    {
        if (Pair.Key.IsValid())
        {
            Pair.Key->SetVisibility(Pair.Value);
        }
    }
    this->HiddenWidgets.Reset();

    UE_LOG(LogTemp, Log, TEXT("[ATutorialSequenceTrigger::OnSequenceFinished()] %s : 시퀀스 종료, 조작 반환"), *GetActorNameOrLabel());

    if (IsValid(this->CheckpointOnFinish))
    {
        this->CheckpointOnFinish->RegisterAfterSequence();
    }

    if (this->bShowMapNameOnEnd)
    {
        UMapNameSubsystem::ShowMapName(this);
    }

    this->OnSequenceEnded.Broadcast(this);
}

FName ATutorialSequenceTrigger::GetSeenSequenceKey() const
{
    const ULevelSequence* Sequence = GetSequenceAsset();
    return Sequence ? Sequence->GetPackage()->GetFName() : NAME_None;
}

void ATutorialSequenceTrigger::MarkSequenceSeen() const
{
    if (!this->bAllowSkip)
    {
        return;
    }
    if (UGameSaveSubsystem* SaveSubsystem = UGameSaveSubsystem::Get(this))
    {
        SaveSubsystem->MarkSequenceSeen(GetSeenSequenceKey());
    }
}

void ATutorialSequenceTrigger::BeginSkipInput()
{
    APlayerCharacterController* CharacterController = Cast<APlayerCharacterController>(this->BlockedController.Get());
    if (!this->bAllowSkip || this->bSkipEnabled || !IsValid(CharacterController))
    {
        return;
    }

    // 처음 보는 컷신은 건너뛸 수 없다. 이때는 건너뛰기 위젯도 뜨지 않고 ESC는 일시정지 메뉴가 받는다
    const UGameSaveSubsystem* SaveSubsystem = UGameSaveSubsystem::Get(this);
    if (!SaveSubsystem || !SaveSubsystem->HasSeenSequence(GetSeenSequenceKey()))
    {
        return;
    }

    // 입력·누른 시간·표시는 HUD의 건너뛰기 위젯이 맡는다. 여기서는 켜고, 끝까지 눌렀다는 요청만 받는다
    this->bSkipEnabled = true;
    this->SkipRequestHandle = CharacterController->OnCinematicSkipRequested.AddUObject(this, &ATutorialSequenceTrigger::SkipSequence);
    CharacterController->SetCinematicSkipAvailable(true);

    UE_LOG(LogTemp, Log, TEXT("[ATutorialSequenceTrigger::BeginSkipInput()] %s : 이미 본 시퀀스, 건너뛰기 허용"), *GetActorNameOrLabel());
}

void ATutorialSequenceTrigger::EndSkipInput()
{
    if (!this->bSkipEnabled)
    {
        return;
    }
    this->bSkipEnabled = false;

    if (APlayerCharacterController* CharacterController = Cast<APlayerCharacterController>(this->BlockedController.Get()))
    {
        CharacterController->OnCinematicSkipRequested.Remove(this->SkipRequestHandle);
        CharacterController->SetCinematicSkipAvailable(false);
    }
    this->SkipRequestHandle.Reset();
}

void ATutorialSequenceTrigger::RefreshTriggerInput()
{
    APlayerController* PlayerController = this->BlockedController.Get();
    if (!IsValid(PlayerController))
    {
        return;
    }

    if (!this->bWaitingForQTE || !IsValid(this->QTEAction))
    {
        DisableInput(PlayerController);
        return;
    }

    // 폰 입력은 막혀 있으므로 이 액터의 입력 컴포넌트를 컨트롤러 입력 스택에 올려 QTE 키만 받는다
    EnableInput(PlayerController);
    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(this->InputComponent);
    if (!EnhancedInput)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ATutorialSequenceTrigger::RefreshTriggerInput()] %s : EnhancedInputComponent 없음"), *GetActorNameOrLabel());
        DisableInput(PlayerController);
        return;
    }

    // 입력 처리 중에 불려도 엔진이 실행할 바인딩을 복사해 두므로 여기서 지워도 안전하다
    EnhancedInput->ClearActionBindings();
    EnhancedInput->BindAction(this->QTEAction, ETriggerEvent::Started, this, &ATutorialSequenceTrigger::OnQTEInput);
}

void ATutorialSequenceTrigger::SkipSequence()
{
    ULevelSequencePlayer* SequencePlayer = IsValid(this->SequenceActor) ? this->SequenceActor->GetSequencePlayer() : nullptr;
    if (this->bSkipped || !SequencePlayer)
    {
        return;
    }
    this->bSkipped = true;

    APlayerController* PlayerController = this->BlockedController.Get();

    // QTE를 기다리던 중이면 안내와 테두리를 치운다. 끝 프레임으로 넘어가므로 QTE는 더 이상 오지 않는다
    GetWorldTimerManager().ClearTimer(this->QTETimer);
    GetWorldTimerManager().ClearTimer(this->QTEFreezeTimer);
    if (this->bWaitingForQTE)
    {
        this->bWaitingForQTE = false;
        this->QTEPrompt->SetHiddenInGame(true);
        SetQTEOutlineVisible(false);
    }

    // 나머지 정리는 OnSequenceFinished가 맡는다. 여기서는 건너뛰기 입력과 안내만 먼저 끈다
    EndSkipInput();

    // 마지막 프레임으로 바로 넘어간 뒤 재생을 이어 가면 다음 업데이트에 끝에 닿아,
    // 정상 종료와 같은 OnFinished(또는 Pause At End의 OnPause) 경로로 끝난다
    const FFrameNumber LastFrame = SequencePlayer->GetEndTime().Time.FrameNumber - 1;
    SequencePlayer->SetPlayRate(1.0f);
    SequencePlayer->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(FFrameTime(LastFrame), EUpdatePositionMethod::Jump));
    if (!SequencePlayer->IsPlaying())
    {
        SequencePlayer->Play();
    }

    if (this->SkipFadeInDuration > 0.0f && IsValid(PlayerController) && IsValid(PlayerController->PlayerCameraManager))
    {
        PlayerController->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, this->SkipFadeInDuration, FLinearColor::Black, false, false);
    }

    UE_LOG(LogTemp, Log, TEXT("[ATutorialSequenceTrigger::SkipSequence()] %s : 시퀀스 건너뜀"), *GetActorNameOrLabel());
}
