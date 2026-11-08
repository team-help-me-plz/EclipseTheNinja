

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PlayerCharacterController.generated.h"

class ASideScrollingCharacter;
class UInputMappingContext;
class UPauseMenuWidget;
class USettingsWidget;

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API APlayerCharacterController : public APlayerController
{
	GENERATED_BODY()

protected:
	/** Right stick cursor speed in viewport heights per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Gamepad", meta = (ClampMin = "0"))
	float GamepadCursorSpeed = 0.8f;

	/** Cursor speed multiplier while holding L2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Gamepad", meta = (ClampMin = "0"))
	float GamepadAimCursorSpeedMultiplier = 2.6f;

	/** Reverse the right stick vertical direction during L2 aiming and clone targeting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Gamepad")
	bool bInvertGamepadAimCursorY = true;

	/** Bone/socket used as the origin when aiming or clone targeting starts. Falls back to the pawn eye position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Gamepad")
	FName GamepadAimHeadSocket = TEXT("head");

	/** Distance in front of the head at which the aim cursor starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Gamepad", meta = (ClampMin = "0", Units = "cm"))
	float GamepadAimStartForwardOffset = 500.0f;

	/** Distance in front of the head at which clone targeting starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Gamepad", meta = (ClampMin = "0", Units = "cm"))
	float CloneTargetingStartForwardOffset = 100.0f;

	/** Radial dead zone applied to the raw right stick axes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Gamepad", meta = (ClampMin = "0", ClampMax = "0.95"))
	float GamepadCursorDeadZone = 0.2f;

	virtual void PlayerTick(float DeltaTime) override;

	/** Input mapping context for this player */
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category = "Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Character class to respawn when the possessed pawn is destroyed */
	UPROPERTY(EditAnywhere, Category = "Respawn")
	TSubclassOf<ACharacter> CharacterClass;


public:
	UFUNCTION(BlueprintPure, Category = "Input")
	bool IsUsingGamepad() const { return bUsingGamepad; }
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	void InitializeGamepadAimCursor(bool bForCloneTargeting = false);
	void StartCameraMove();
	void StopCameraMove();

	// 정해진 입력만 받아야 할 때(분신 조준, 은신 등) 이유별로 잠근다. 잠금이 하나라도 있으면 기본 IMC를 빼고,
	// 각 잠금이 넘긴 허용 IMC만 넣는다. 모든 잠금이 풀리면 기본 IMC를 다시 넣는다.
	void PushInputLock(FName InReason, UInputMappingContext* InAllowedContext = nullptr);
	void PopInputLock(FName InReason);
	bool IsInputLocked() const { return !InputLocks.IsEmpty(); }

	void ResumeFromPauseMenu();
	void OpenPauseSettings();
	void ReturnToMainMenu();
	bool HasOpenPauseMenu() const { return PauseMenuWidget != nullptr; }
	void HandlePauseMenuBack();

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;

	/** Pawn initialization */
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** Called if the possessed pawn is destroyed */
	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedActor);

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

private:
	void OpenPauseMenu();
	void FocusPauseWidget(UUserWidget& Widget);
	void RemovePauseWidgets();

	UFUNCTION()
	void HandlePauseSettingsClosed();

	UPROPERTY(Transient)
	TObjectPtr<UPauseMenuWidget> PauseMenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<USettingsWidget> PauseSettingsWidget;

	bool bShowCursorBeforePause = false;
	bool bUsingGamepad = false;
	double IgnoreCursorWarpUntil = 0.0;
	FVector2D LastObservedMousePosition = FVector2D::ZeroVector;
	bool bHasObservedMousePosition = false;
	void SetGamepadCursorLocation(int32 X, int32 Y);
	FVector2D GamepadCursorRemainder = FVector2D::ZeroVector;

	// DefaultMappingContexts와 (터치가 아니면) MobileExcludedMappingContexts를 넣거나 뺀다
	void ApplyDefaultMappingContexts(bool bAdd);
	// 잠긴 채로 캐릭터가 바뀌면 모든 잠금을 풀어 기본 입력으로 되돌린다
	void ClearInputLocks();

	// 잠금 이유와 그 잠금 동안 넣는 허용 IMC(없으면 nullptr)
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UInputMappingContext>> InputLocks;
};
