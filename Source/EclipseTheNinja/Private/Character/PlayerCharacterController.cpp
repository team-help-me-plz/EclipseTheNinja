


#include "Character/PlayerCharacterController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Character/PlayerCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "EclipseTheNinja.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "Character/PlayerCharacterCameraManager.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "Components/InputComponent.h"
#include "Widgets/PauseMenuWidget.h"
#include "Widgets/SettingsWidget.h"


void APlayerCharacterController::BeginPlay()
{
    Super::BeginPlay();

    SetShowMouseCursor(true);
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

void APlayerCharacterController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (InputComponent)
    {
        // 게임플레이 IMC가 잠긴 상태에서도 ESC로 메뉴를 열 수 있다.
        InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &APlayerCharacterController::OpenPauseMenu);
    }

    // add the input mapping context
    ApplyDefaultMappingContexts(true);

    // only spawn touch controls on local player controllers
    if (IsLocalPlayerController() && ShouldUseTouchControls())
    {
        // spawn the mobile controls widget
        MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

        if (MobileControlsWidget)
        {
            // add the controls to the player screen
            MobileControlsWidget->AddToPlayerScreen(0);

        }
        else
        {

            UE_LOG(LogEclipseTheNinja, Error, TEXT("Could not spawn mobile controls widget."));

        }

    }
}

void APlayerCharacterController::OpenPauseMenu()
{
    if (!IsLocalPlayerController()) return;
    if (PauseMenuWidget) return;
    if (IsPaused()) return;

    UPauseMenuWidget* Menu = CreateWidget<UPauseMenuWidget>(this);
    if (!Menu) return;
    if (!SetPause(true)) return;

    if (!Menu->AddToPlayerScreen(10))
    {
        SetPause(false);
        return;
    }

    PauseMenuWidget = Menu;
    bShowCursorBeforePause = ShouldShowMouseCursor();
    FocusPauseWidget(*Menu);
    SetShowMouseCursor(true);
}

void APlayerCharacterController::FocusPauseWidget(UUserWidget& Widget)
{
    // 게임플레이 IMC를 그대로 유지하면서 입력은 현재 모달에만 전달한다.
    Widget.SetCursor(EMouseCursor::Default);
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(Widget.TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    Widget.SetUserFocus(this);
}

void APlayerCharacterController::ResumeFromPauseMenu()
{
    if (!PauseMenuWidget) return;
    if (IsPaused() && !SetPause(false)) return;

    RemovePauseWidgets();

    // 게임플레이의 커서 조준과 기존 입력 잠금을 유지한다.
    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    SetShowMouseCursor(bShowCursorBeforePause);
    FlushPressedKeys();
}

void APlayerCharacterController::OpenPauseSettings()
{
    if (!PauseMenuWidget) return;
    if (PauseSettingsWidget) return;

    USettingsWidget* Settings = CreateWidget<USettingsWidget>(this);
    if (!Settings) return;
    if (!Settings->AddToPlayerScreen(11)) return;

    PauseSettingsWidget = Settings;
    Settings->OnSettingsClosed.AddUniqueDynamic(this, &APlayerCharacterController::HandlePauseSettingsClosed);
    PauseMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
    FocusPauseWidget(*Settings);
}

void APlayerCharacterController::HandlePauseSettingsClosed()
{
    PauseSettingsWidget = nullptr;
    if (!PauseMenuWidget) return;

    PauseMenuWidget->SetVisibility(ESlateVisibility::Visible);
    FocusPauseWidget(*PauseMenuWidget);
}

void APlayerCharacterController::HandlePauseMenuBack()
{
    if (PauseSettingsWidget)
    {
        PauseSettingsWidget->CloseSettings();
        return;
    }

    ResumeFromPauseMenu();
}

void APlayerCharacterController::ReturnToMainMenu()
{
    if (!PauseMenuWidget) return;

    ResumeFromPauseMenu();
    if (PauseMenuWidget) return;

    UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Maps/Lv_MainMenu")));
}

void APlayerCharacterController::RemovePauseWidgets()
{
    if (PauseSettingsWidget)
    {
        PauseSettingsWidget->OnSettingsClosed.RemoveDynamic(this, &APlayerCharacterController::HandlePauseSettingsClosed);
        PauseSettingsWidget->RemoveFromParent();
        PauseSettingsWidget = nullptr;
    }
    if (PauseMenuWidget)
    {
        PauseMenuWidget->RemoveFromParent();
        PauseMenuWidget = nullptr;
    }
}

void APlayerCharacterController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemovePauseWidgets();
    Super::EndPlay(EndPlayReason);
}

void APlayerCharacterController::ApplyDefaultMappingContexts(bool bAdd)
{
    // only add IMCs for local player controllers
    if (!IsLocalPlayerController()) return;

    UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
    if (!Subsystem) return;

    TArray<UInputMappingContext*> Contexts = DefaultMappingContexts;
    // only add these IMCs if we're not using mobile touch input
    if (!ShouldUseTouchControls())
    {
        Contexts.Append(MobileExcludedMappingContexts);
    }

    // 다시 넣을 때 눌려 있던 키(분신 확정 좌클릭 등)가 바로 다른 행동으로 이어지지 않게 한다
    FModifyContextOptions Options;
    Options.bIgnoreAllPressedKeysUntilRelease = true;
    Options.bNotifyUserSettings = bAdd;
    for (UInputMappingContext* CurrentContext : Contexts)
    {
        if (!CurrentContext) continue;
        if (bAdd)
        {
            Subsystem->AddMappingContext(CurrentContext, 0, Options);
        }
        else
        {
            Subsystem->RemoveMappingContext(CurrentContext, Options);
        }
    }
}

void APlayerCharacterController::PushInputLock(FName InReason, UInputMappingContext* InAllowedContext)
{
    if (InputLocks.Contains(InReason)) return;

    const bool bWasUnlocked = InputLocks.IsEmpty();
    InputLocks.Add(InReason, InAllowedContext);
    if (bWasUnlocked)
    {
        ApplyDefaultMappingContexts(false);
    }

    UEnhancedInputLocalPlayerSubsystem* Subsystem = IsLocalPlayerController()
        ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()) : nullptr;
    if (Subsystem && InAllowedContext)
    {
        // 잠금을 건 입력(은신 진입의 E 등)이 눌린 채로 허용 IMC에서 다시 들어오지 않게 한다
        FModifyContextOptions Options;
        Options.bIgnoreAllPressedKeysUntilRelease = true;
        Subsystem->AddMappingContext(InAllowedContext, 0, Options);
    }
}

void APlayerCharacterController::PopInputLock(FName InReason)
{
    TObjectPtr<UInputMappingContext> AllowedContext = nullptr;
    if (!InputLocks.RemoveAndCopyValue(InReason, AllowedContext)) return;

    UEnhancedInputLocalPlayerSubsystem* Subsystem = IsLocalPlayerController()
        ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()) : nullptr;
    // 다른 잠금이 같은 IMC를 쓰고 있으면 남겨 둔다
    TArray<TObjectPtr<UInputMappingContext>> RemainingContexts;
    InputLocks.GenerateValueArray(RemainingContexts);
    if (Subsystem && AllowedContext && !RemainingContexts.Contains(AllowedContext))
    {
        Subsystem->RemoveMappingContext(AllowedContext);
    }

    if (InputLocks.IsEmpty())
    {
        ApplyDefaultMappingContexts(true);
    }
}

void APlayerCharacterController::ClearInputLocks()
{
    TArray<FName> Reasons;
    InputLocks.GetKeys(Reasons);
    for (const FName& Reason : Reasons)
    {
        PopInputLock(Reason);
    }
}

void APlayerCharacterController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    // 잠근 채로 이전 캐릭터가 사라졌으면 새 캐릭터는 기본 입력으로 시작한다
    ClearInputLocks();

    // subscribe to the pawn's OnDestroyed delegate
    InPawn->OnDestroyed.AddUniqueDynamic(this, &APlayerCharacterController::OnPawnDestroyed);
}

void APlayerCharacterController::OnUnPossess()
{
    if (APawn* PreviousPawn = GetPawn())
    {
        if (!PreviousPawn->IsActorBeingDestroyed())
        {
            PreviousPawn->OnDestroyed.RemoveDynamic(this, &APlayerCharacterController::OnPawnDestroyed);
        }
    }
    StopCameraMove();
    Super::OnUnPossess();
}

void APlayerCharacterController::OnPawnDestroyed(AActor* DestroyedActor)
{
    if (!HasAuthority() || !IsValid(DestroyedActor) || GetWorld()->bIsTearingDown) return;
    // A missing Blueprint override should still respawn the same configured player.
    const TSubclassOf<ACharacter> RespawnClass = CharacterClass ? CharacterClass.Get() : DestroyedActor->GetClass();

    if (UCheckpointSubsystem* Subsystem = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
    {
        if (Subsystem->HasValidCheckpoint())
        {
            // 시체 위치에 스폰하면 가시 함정 같은 트리거와 겹친 채 BeginPlay 직후 초기 오버랩이 발생해
            // 새 캐릭터가 피해를 입은 상태로 체크포인트로 옮겨진다. 처음부터 체크포인트 위치에 스폰한다.
            FTransform SpawnTransform = DestroyedActor->GetTransform();
            Subsystem->GetCheckpointTransform(SpawnTransform);

            if (APlayerCharacter* RespawnedCharacter = GetWorld()->SpawnActor<APlayerCharacter>(RespawnClass, SpawnTransform))
            {
                Possess(RespawnedCharacter);
                Subsystem->RespawnAtCheckpoint(RespawnedCharacter);
                return;
            }
        }
    }

    // find the player start
    TArray<AActor*> ActorList;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), ActorList);

    if (ActorList.Num() > 0)
    {
        // spawn a character at the player start
        const FTransform SpawnTransform = ActorList[0]->GetActorTransform();

        if (APlayerCharacter* RespawnedCharacter = GetWorld()->SpawnActor<APlayerCharacter>(RespawnClass, SpawnTransform))
        {
            UE_LOG(LogTemp, Display, TEXT("RespawnTest"));
            // possess the character
            Possess(RespawnedCharacter);
        }
    }
}

bool APlayerCharacterController::ShouldUseTouchControls() const
{
    // are we on a mobile platform? Should we force touch?
    return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void APlayerCharacterController::StartCameraMove()
{
    APlayerCharacterCameraManager* ACM = Cast<APlayerCharacterCameraManager>(PlayerCameraManager);
    if (ACM)
        ACM->SetCameraExploreMode(true);
}

void APlayerCharacterController::StopCameraMove()
{
    APlayerCharacterCameraManager* ACM = Cast<APlayerCharacterCameraManager>(PlayerCameraManager);
    if (ACM)
        ACM->SetCameraExploreMode(false);
}
