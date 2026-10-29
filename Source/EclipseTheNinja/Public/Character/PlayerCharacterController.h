

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PlayerCharacterController.generated.h"

class ASideScrollingCharacter;
class UInputMappingContext;

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API APlayerCharacterController : public APlayerController
{
	GENERATED_BODY()

protected:

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
	void StartCameraMove();
	void StopCameraMove();

	// 정해진 입력만 받아야 할 때(분신 조준, 은신 등) 이유별로 잠근다. 잠금이 하나라도 있으면 기본 IMC를 빼고,
	// 각 잠금이 넘긴 허용 IMC만 넣는다. 모든 잠금이 풀리면 기본 IMC를 다시 넣는다.
	void PushInputLock(FName InReason, UInputMappingContext* InAllowedContext = nullptr);
	void PopInputLock(FName InReason);
	bool IsInputLocked() const { return !InputLocks.IsEmpty(); }

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

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
	// DefaultMappingContexts와 (터치가 아니면) MobileExcludedMappingContexts를 넣거나 뺀다
	void ApplyDefaultMappingContexts(bool bAdd);
	// 잠긴 채로 캐릭터가 바뀌면 모든 잠금을 풀어 기본 입력으로 되돌린다
	void ClearInputLocks();

	// 잠금 이유와 그 잠금 동안 넣는 허용 IMC(없으면 nullptr)
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UInputMappingContext>> InputLocks;
};
