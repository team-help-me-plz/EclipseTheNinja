


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

    // only add IMCs for local player controllers
    if (IsLocalPlayerController())
    {
        // add the input mapping context
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
        {
            for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
            {
                Subsystem->AddMappingContext(CurrentContext, 0);
            }

            // only add these IMCs if we're not using mobile touch input
            if (!ShouldUseTouchControls())
            {
                for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
                {
                    Subsystem->AddMappingContext(CurrentContext, 0);
                }
            }
        }
    }

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

void APlayerCharacterController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

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
            if (APlayerCharacter* RespawnedCharacter = GetWorld()->SpawnActor<APlayerCharacter>(RespawnClass, DestroyedActor->GetTransform()))
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
