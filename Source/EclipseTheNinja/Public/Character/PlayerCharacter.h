// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "Abilities/GameplayAbility.h"
#include "PlayerCharacter.generated.h"

class UStatAttributeSet;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class UAnimMontage;
class UGABaseAttack;
struct FInputActionValue;

/**
 *  공통 전투 기능을 상속받아 카메라, 플레이어 입력, 점프를 처리한다.
 */
UCLASS()
class ECLIPSETHENINJA_API APlayerCharacter : public ABaseCharacter
{
	GENERATED_BODY()

	/** Player camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Camera", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* Camera;

protected:

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Drop from Platform Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* DropAction;

	/** Interact Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* InteractAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> BasicAttackAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> AimAction;


	/** Impulse to manually push physics objects while we're in midair */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Jump")
	float JumpPushImpulse = 600.0f;

	/** Max distance that interactive objects can be triggered */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Interaction")
	float InteractionRadius = 200.0f;

	/** Time to disable input after a wall jump to preserve momentum */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Wall Jump")
	float DelayBetweenWallJumps = 0.3f;

	/** Distance to trace ahead of the character for wall jumps */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Wall Jump")
	float WallJumpTraceDistance = 50.0f;

	/** Horizontal impulse to apply to the character during wall jumps */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Wall Jump")
	float WallJumpHorizontalImpulse = 500.0f;

	/** Multiplies the jump Z velocity for wall jumps. */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Wall Jump")
	float WallJumpVerticalMultiplier = 1.4f;

	/** Collision object type to use for soft collision traces (dropping down floors) */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Soft Platforms")
	TEnumAsByte<ECollisionChannel> SoftCollisionObjectType;

	/** Distance to trace down during soft collision checks */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Soft Platforms")
	float SoftCollisionTraceDistance = 1000.0f;

	/** Last recorded time when this character started falling */
	float LastFallTime = 0.0f;

	/** Max amount of time that can pass since we started falling when we allow a regular jump */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Coyote Time", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float MaxCoyoteTime = 0.16f;

	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> AttackInputSubsystem;

	/** Wall jump lockout timer */
	FTimerHandle WallJumpTimer;

	/** Last captured horizontal movement input value */
	float ActionValueY = 0.0f;

	/** Last captured platform drop axis value */
	float DropValue = 0.0f;

	/** If true, this character has already wall jumped */
	bool bHasWallJumped = false;

	/** True only while the initial walk-off coyote jump is still available. */
	bool bCanCoyoteJump = false;

	/** If true, this character is moving along the side scrolling axis */
	bool bMovingHorizontally = false;

public:
	
	/** Constructor */
	APlayerCharacter();
	virtual void DoBasicAttack() override;

	bool HasCursorTarget() const { return bHasCursorTarget; }
	bool IsFacingCursor() const { return bCursorFacingActive; }
	const FVector& GetCursorWorldLocation() const { return CursorWorldLocation; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor Look")
	bool bFaceCursor = true;

	// 중심 근처에서 방향이 계속 뒤집히는 것을 막는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cursor Look", meta = (ClampMin = "0", Units = "cm"))
	float CursorFacingDeadZone = 15.0f;

	// 초당 회전 각도. 360이면 180도 회전에 약 0.5초가 걸린다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cursor Look", meta = (ClampMin = "1", Units = "deg/s"))
	float CursorBodyTurnSpeed = 360.0f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void UpdateCursorTarget();
	void UpdateCursorFacing(float DeltaSeconds);
	void SetCursorFacingActive(bool bActive);

	FVector CursorWorldLocation = FVector::ZeroVector;
	bool bHasCursorTarget = false;
	bool bCursorFacingActive = false;
	float CursorFacingYaw = 0.0f;
	bool bSavedOrientRotationToMovement = false;
	bool bSavedUseControllerDesiredRotation = false;
	bool bSavedUseControllerRotationYaw = false;

protected:

	/** Gameplay cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Collision handling */
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	/** Landing handling */
	virtual void Landed(const FHitResult& Hit) override;

	/** Handle movement mode changes to keep track of coyote time jumps */
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

	/**캐릭터 빙의시 호출되는 함수, ASC 활성화 하는 타이밍으로 사용**/
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void PawnClientRestart() override;


protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for drop from platform input */
	void Drop(const FInputActionValue& Value);

	/** Called for drop from platform input release */
	void DropReleased(const FInputActionValue& Value);

	// 체력 어트리뷰트가 변경되었을 때 호출될 함수
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	// 사망 처리용 함수
	UFUNCTION(BlueprintCallable, Category = "Combat|Death")
	void Die();

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Forward);

	/** Handles drop inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoDrop(float Value);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/** Handles interact inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoInteract();


	// 카메라 이동 처리
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoCameraMoveStart();

	// 카메라 이동 처리
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoCameraMoveEnd();

protected:

	/** 발판 내려가기, 지상 점프, 벽 점프, 코요테 점프를 처리한다. */
	void MultiJump();

	/** Checks for soft collision with platforms */
	void CheckForSoftCollision();

	/** Resets wall jump lockout. Called from timer after a wall jump */
	void ResetWallJump();

public:

	/** Sets the soft collision response. True passes, False blocks */
	void SetSoftCollision(bool bEnabled);

public:

	/** 기존 Anim BP 연결을 유지하기 위한 함수. 2단 점프는 사용하지 않는다. */
	UFUNCTION(BlueprintPure, Category="Side Scrolling")
	bool HasDoubleJumped() const { return false; }

	/** Returns true if the character has just wall jumped */
	UFUNCTION(BlueprintPure, Category="Side Scrolling")
	bool HasWallJumped() const;



	/*============================================================
	여기 있는 코드는 단순 테스트용 실제 빌드에서는 사용하지 마시오
	=============================================================*/
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AttackTest")
	TSubclassOf<UGameplayAbility> TestAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackTest", meta = (ClampMin = "1"))
	int32 TestAbilityLevel = 1;

	UFUNCTION(BlueprintCallable)
	void GiveTestAbility();

};
