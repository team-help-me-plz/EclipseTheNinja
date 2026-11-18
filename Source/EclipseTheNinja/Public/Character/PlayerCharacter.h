// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "Abilities/GameplayAbility.h"
#include "GAS/AbilityInfo.h"
#include "Interface/Interactor.h"
#include "Character/PlayerCondition.h"
#include "GenericPlatform/ICursor.h"
#include "PlayerCharacter.generated.h"

class UStatAttributeSet;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class UAnimMontage;
class UGABaseAttack;
class UInteractBoxComponent;
class UAssassinationTargetingComponent;
class UNoiseComponent;
class UHidingComponent;
class UMinimapComponent;
class UPointLightComponent;
class UGAAssassinate;
class AEnemy;
class UUserWidget;
class USoundBase;
class UNiagaraSystem;
struct FInputActionValue;

UENUM()
enum class EPlayerActionSound : uint8
{
	Kunai, SwiftStrike, Parry, Clone, BelowAssassination, Jump, Land, Hit, AssassinationStab, ParrySuccess, LedgeClimb,
	// 분신 회수
	CloneRecall,
	// 분신과 위치 교환
	CloneSwap,
	// 분신 협동 점프로 날아오를 때
	CloneCoopJump
};

DECLARE_MULTICAST_DELEGATE_OneParam(FPlayerConditionChanged, EPlayerCondition);

/**
 *  공통 전투 기능을 상속받아 카메라, 플레이어 입력, 점프를 처리한다.
 */
UCLASS()
class ECLIPSETHENINJA_API APlayerCharacter : public ABaseCharacter, public IInteractor
{
	GENERATED_BODY()

	/** Player camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* Camera;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TMap<EPlayerActionSound, TObjectPtr<USoundBase>> ActionSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TArray<TObjectPtr<USoundBase>> SwordAttackSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TArray<TObjectPtr<USoundBase>> FireAttackSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TArray<TObjectPtr<USoundBase>> AssassinationSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TArray<TObjectPtr<USoundBase>> WalkSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TArray<TObjectPtr<USoundBase>> CrouchWalkSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TArray<TObjectPtr<USoundBase>> WallClimbSounds;

	/** 걸을 때 발소리 한 번을 내는 이동 거리(cm) */
	UPROPERTY(EditDefaultsOnly, Category = "Audio", meta = (ClampMin = "1.0", Units = "cm"))
	float WalkStepDistance = 130.0f;

	/** 웅크리고 걸을 때 발소리 한 번을 내는 이동 거리(cm) */
	UPROPERTY(EditDefaultsOnly, Category = "Audio", meta = (ClampMin = "1.0", Units = "cm"))
	float CrouchWalkStepDistance = 80.0f;

	/** 벽을 탈 때 소리 한 번을 내는 세로 이동 거리(cm) */
	UPROPERTY(EditDefaultsOnly, Category = "Audio", meta = (ClampMin = "1.0", Units = "cm"))
	float WallClimbStepDistance = 75.0f;

	/** 분신과 위치를 교환할 때 두 자리(플레이어·분신)에서 각각 재생할 연기 이펙트. 비우면 재생하지 않는다 */
	UPROPERTY(EditDefaultsOnly, Category = "Effect|Clone")
	TObjectPtr<UNiagaraSystem> CloneSwapSmokeSystem;

	/** 교환 연기 이펙트를 액터 위치에서 얼마나 옮겨 재생할지(월드 기준) */
	UPROPERTY(EditDefaultsOnly, Category = "Effect|Clone")
	FVector CloneSwapSmokeOffset = FVector::ZeroVector;

	/** 0보다 크면 이 시간(초) 뒤 새 파티클 생성을 멈춘다. 루프 이펙트를 쓸 때 지정한다. 0이면 이펙트가 스스로 끝날 때까지 둔다 */
	UPROPERTY(EditDefaultsOnly, Category = "Effect|Clone", meta = (ClampMin = "0.0", Units = "s"))
	float CloneSwapSmokeDuration = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision|Interact")
	TObjectPtr<UInteractBoxComponent> InteractComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collision|Interact")
	FVector InteractBoxSize = FVector(80.f, 1000.f, 100.f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Assassinate")
	TObjectPtr<UAssassinationTargetingComponent> AssassinateTargetingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Noise")
	TObjectPtr<UNoiseComponent> NoiseComponent;

	// 켜면 칼 메시의 소켓(AttackNoiseSocketName)에서, 끄면 캐릭터 스켈레탈 메시의 중심에서 공격 소음이 난다.
	// 소켓을 가진 메시를 찾지 못하면 캐릭터 메시 중심을 쓴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Noise|Attack", meta = (DisplayName = "Attack Noise From Sword Socket"))
	bool bAttackNoiseFromSwordSocket = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Noise|Attack", meta = (EditCondition = "bAttackNoiseFromSwordSocket"))
	FName AttackNoiseSocketName = TEXT("b_TrailSocket");

	// 은신구역에 들어가고 나오는 연출을 맡는다. 숨기 몽타주는 여기에 지정한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hiding")
	TObjectPtr<UHidingComponent> HidingComponent;

	// 미니맵 시야 밝히기와 Tab 입력, 미니맵 위젯 표시를 맡는다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
	TObjectPtr<UMinimapComponent> MinimapComponent;

	// 플레이어 주변을 은은하게 밝히는 빛. 밝기·색·반경은 BP에서 조절한다. 숨어 있는 동안에는 꺼진다.
	// 그림자는 항상 만들지 않는다(BeginPlay에서 다시 끈다).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<UPointLightComponent> AuraLight;

	// 숨기 시작하거나 나올 때 빛이 꺼지고 켜지는 데 걸리는 시간. 0이면 바로 바뀐다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light", meta = (ClampMin = "0", Units = "s"))
	float AuraLightFadeTime = 0.3f;

	// 플레이어 위치에서 AuraLight까지의 월드 기준 거리. 플레이어가 좌우로 돌아도 이 위치를 유지한다.
	// Y +는 카메라 쪽이라 정면에서 고르게 비춰 몸의 명암을 줄인다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light")
	FVector AuraLightOffset = FVector(0.0f, 100.0f, 40.0f);

	// 예전 BP에 저장된 암살 클래스. PostLoad에서 어빌리티 배열로 옮긴다.
	UPROPERTY()
	TSubclassOf<UGAAssassinate> AssassinationAbilityClass_DEPRECATED;

	virtual void MigrateLegacyAbilities() override;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MoveAction;

	/** Minimum downward movement value required to start hanging from a ledge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Gamepad", meta = (ClampMin = "0.1", ClampMax = "1"))
	float LedgeHangDownInputThreshold = 0.65f;

	/** 좌우 이동 입력이 이 값 이하이면 입력이 없는 것으로 본다. 스틱을 살짝 건드려도 움직이지 않게 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Gamepad", meta = (ClampMin = "0", ClampMax = "0.9"))
	float MoveInputDeadZone = 0.2f;

	/**
	 * 데드존을 넘은 이동 입력의 최소 배율. 이동 속도는 (배율 x 최대 속도)이므로 서 있을 때와 앉았을 때 모두 같은 비율이 적용된다.
	 * 걷기 애니메이션이 제대로 재생되어 발소리 노티파이가 나는 속도 이상이 되도록 맞춘다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Gamepad", meta = (ClampMin = "0", ClampMax = "1"))
	float MinMoveInputScale = 0.5f;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* JumpAction;

	/** Drop from Platform Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* DropAction;

	/** Interact Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* InteractAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> BasicAttackAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> AimAction;

	/** CloneTech Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CloneTechAction;

	/** CloneDespawn Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CloneRecallAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveScrollAction;

	/** Crouch Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CrouchAction;

	/** ElementalFire Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ElementalFireAction;

	/** SwiftStrike Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SwiftStrikeAction;

	/** Impulse to manually push physics objects while we're in midair */
	UPROPERTY(EditAnywhere, Category = "Side Scrolling|Jump")
	float JumpPushImpulse = 600.0f;

	/** Max distance that interactive objects can be triggered */
	UPROPERTY(EditAnywhere, Category = "Side Scrolling|Interaction")
	float InteractionRadius = 200.0f;

	/** Time to disable input after a wall jump to preserve momentum */
	UPROPERTY(EditAnywhere, Category = "Side Scrolling|Wall Jump")
	float DelayBetweenWallJumps = 0.3f;

	/** Distance to trace ahead of the character for wall jumps */
	UPROPERTY(EditAnywhere, Category = "Side Scrolling|Wall Jump")
	float WallJumpTraceDistance = 50.0f;

	/** Horizontal impulse to apply to the character during wall jumps */
	UPROPERTY(EditAnywhere, Category = "Side Scrolling|Wall Jump")
	float WallJumpHorizontalImpulse = 500.0f;

	/** Multiplies the jump Z velocity for wall jumps. */
	UPROPERTY(EditAnywhere, Category = "Side Scrolling|Wall Jump")
	float WallJumpVerticalMultiplier = 1.4f;

	/** Collision object type to use for soft collision traces (dropping down floors) */
	UPROPERTY(EditAnywhere, Category = "Side Scrolling|Soft Platforms")
	TEnumAsByte<ECollisionChannel> SoftCollisionObjectType;

	/** Distance to trace down during soft collision checks */
	UPROPERTY(EditAnywhere, Category = "Side Scrolling|Soft Platforms")
	float SoftCollisionTraceDistance = 1000.0f;

	/** Last recorded time when this character started falling */
	float LastFallTime = 0.0f;

	/** Max amount of time that can pass since we started falling when we allow a regular jump */
	UPROPERTY(EditAnywhere, Category = "Side Scrolling|Coyote Time", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float MaxCoyoteTime = 0.16f;

	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> AttackInputSubsystem;

	/** Wall jump lockout timer */
	FTimerHandle WallJumpTimer;

	/** Last captured horizontal movement input value */
	float ActionValueY = 0.0f;

	// 지금 누르고 있는 좌우 이동 입력 방향(-1, 0, 1). 키를 떼면 0이 된다
	float HeldMoveInputSign = 0.0f;

	// 첫 공격을 시작할 때 이미 누르고 있던 이동 방향. 이 방향을 계속 누르는 것으로는 공격이 끊기지 않는다
	float AttackStartMoveInputSign = 0.0f;

	/** If true, this character has already wall jumped */
	bool bHasWallJumped = false;

	/** True only while the initial walk-off coyote jump is still available. */
	bool bCanCoyoteJump = false;

	/** If true, this character is moving along the side scrolling axis */
	bool bMovingHorizontally = false;

	/** 외부에서 직접 바꾸지 않는다. AddEngagedEnemy, TryEnterHiding 같은 의도별 함수로만 바뀐다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Condition")
	EPlayerCondition	PlayerCondition = EPlayerCondition::Normal;

public:
	void PlayActionSound(EPlayerActionSound Action, float Volume = 1.0f) const;
	// ActionSounds에 지정한 사운드를 돌려준다. 없으면 nullptr
	USoundBase* GetActionSound(EPlayerActionSound Action) const;
	// 분신 위치 교환 연기 이펙트를 InLocation(+CloneSwapSmokeOffset)에서 재생한다
	void PlayCloneSwapSmoke(const FVector& InLocation) const;
	void PlaySwordAttackSound(int32 ComboIndex, bool bFire) const;
	// 공격 SFX와 같은 타이밍에 부른다. 발생 위치는 bAttackNoiseFromSwordSocket으로 고른다.
	void EmitAttackNoise();
	FVector GetAttackNoiseLocation() const;
	void PlayAssassinationWithdrawalSound() const;

    const UInputAction* GetAbilityInputAction(EAbilityInputID InputID) const;
    const UInputAction* GetCloneRecallInputAction() const { return CloneRecallAction; }

    float GetLogicalFacingSign() const { return FMath::Cos(FMath::DegreesToRadians(FacingTargetYaw)) >= 0.0f ? 1.0f : -1.0f; }
    UFUNCTION(BlueprintCallable, Category="Parry")
    void DoParry();
    bool IsParrying() const;
    void SetParryMovementLocked(bool bLocked);
    UPROPERTY(EditAnywhere, Category="Input")
    TObjectPtr<UInputAction> ParryAction;

    /**
     * 디버그용 무적. 켜면 질풍참과 같은 State.Invincible 태그를 추가한다.
     * BP 기본값이나 레벨 인스턴스에서 켜 두거나, PIE 중 아웃라이너에서 플레이어를 골라 디테일 패널에서 바로 바꿀 수 있다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bDebugInvincible = false;

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void SetDebugInvincible(bool bEnable);

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    void ApplyUnlockedAbilities(const TArray<EAbilityInputID>& Unlocked);

    // Future unlockable abilities are configured in Blueprint defaults, separately from startup abilities.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Unlocks")
    TArray<FAbilityInfo> UnlockableAbilities;

	/** Constructor */
	APlayerCharacter();
	virtual void DoBasicAttack() override;
	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoAssassinate();

	// 표창 투척은 카메라 이동 모드(조준 모드)에서만 허용하며, 분신 타게팅의 좌클릭 확인, 암살 연출 재생 시에는 투척할 수 없도록 한다.
	UFUNCTION(BlueprintPure, Category = "Combat|Kunai")
	bool CanThrowKunai() const { return bIsExploring && !bIsCloneTargeting && !bIsDead && !IsAssassinating() && !IsHiding(); }

	// 표창 투척 어빌리티를 실행하는 함수
	UFUNCTION(BlueprintCallable, Category = "Combat|Kunai")
	void DoThrowKunai();

	// 현재 커서 쪽(좌우)으로 몸을 즉시 돌린다. 표창 투척 어빌리티가 실행될 때 호출한다.
	void FaceCursorForThrow();

	// 우클릭 조준 중 마우스 위치에 띄우는 조준 위젯.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Kunai|Aim")
	TSubclassOf<UUserWidget> AimWidgetClass;

	// 조준 위젯이 떠 있는 동안 OS 마우스 커서를 숨겨 조준 위젯만 보이게 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Kunai|Aim")
	bool bHideMouseCursorWhileAiming = true;

	virtual void RegisterSafePoint_Implementation(const ATargetPoint* InSafePoint) override;
	virtual void ReturnToSafePoint_Implementation() override;
	virtual void PlayInteractMontage_Implementation(AActor* InInteractable, UAnimMontage* InMontage) override;

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoCloneTech();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoCloneTechEnd();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoCloneRecall();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoMouseScroll(const FInputActionValue& InValue);

	// 칼에 불속성을 부여하는 어빌리티를 실행하는 함수
	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoElementalFire();

	bool HasCursorTarget() const { return bHasCursorTarget; }
	bool IsFacingCursor() const { return bCursorFacingActive; }
	const FVector& GetCursorWorldLocation() const { return CursorWorldLocation; }

	UAssassinationTargetingComponent* GetAssassinationTargetingComponent() const { return AssassinateTargetingComponent; }
	bool IsAssassinating() const;
	// 지금 상호작용 키를 누르면 암살이 실행되는지. 상호작용보다 우선한다.
	bool CanAssassinateCurrentTarget() const;
	bool IsWallAttached() const;

	// 은신구역에 들어가는 중, 숨어 있는 중, 나오는 중 모두 true.
	// 이 동안의 입력은 HidingComponent가 컨트롤러 IMC를 바꿔서 제한한다.
	bool IsHiding() const;
	void SetAssassinationFacing(float Yaw);

	// 보스 처형처럼 적에게 암살당할 때 호출한다. 사망 몽타주 대신 Montage를 재생하며 사망한다.
	void ReceiveAssassination(UAnimMontage* Montage);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor Look")
	bool bFaceCursor = true;

	// 중심 근처에서 방향이 계속 뒤집히는 것을 막는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cursor Look", meta = (ClampMin = "0", Units = "cm"))
	float CursorFacingDeadZone = 15.0f;

	// 초당 회전 각도. 360이면 180도 회전에 약 0.5초가 걸린다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cursor Look", meta = (ClampMin = "1", Units = "deg/s"))
	float CursorBodyTurnSpeed = 360.0f;

	// A/D를 짧게 눌러 회전이 중간에 끊겼을 때, 남은 각도가 이 값 이상이면 턴 클립의 남은 구간으로 마무리한다.
	// 이보다 작으면 이동 회전 속도로 돌린다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0", ClampMax = "180", Units = "deg"))
	float MoveTurnClipMinYaw = 30.0f;

	// 이동 입력으로 턴 클립을 끊을 때의 블렌드 시간.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0", Units = "s"))
	float MoveTurnBlendOutTime = 0.15f;

	// 점프 시 소리를 발생시키기 위해 Character에 존재하는 함수 오버라이드
	virtual void OnJumped_Implementation() override;

	/** PlayerCondition이 바뀔 때만 알린다. 전투로 은신이 강제 해제될 때도 여기서 Combat을 받는다. */
	FPlayerConditionChanged OnPlayerConditionChanged;

	UFUNCTION(BlueprintPure, Category = "Condition")
	EPlayerCondition	GetPlayerCondition() const	{ return (this->PlayerCondition); }

	// 적이 플레이어를 전투 대상으로 삼거나 놓았을 때 호출한다. 같은 적을 여러 번 넣어도 한 번만 센다.
	void	AddEngagedEnemy(AEnemy* InEnemy);
	void	RemoveEngagedEnemy(AEnemy* InEnemy);

	// 은신 지점에서 호출한다. 전투 중이면 false를 반환하고 숨지 않는다.
	bool	TryEnterHiding();
	void	ExitHiding();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void PlayCharacterSound(USoundBase* Sound, float Volume = 1.0f) const;
	void PlayRandomCharacterSound(const TArray<TObjectPtr<USoundBase>>& Sounds) const;
	void UpdateMovementSound(float DeltaSeconds);
	float DistanceSinceMovementSound = 0.0f;
	const TArray<TObjectPtr<USoundBase>>* PreviousMovementSounds = nullptr;

	// 숨기 상태에 맞춰 AuraLight 밝기를 서서히 0 또는 원래 값으로 맞춘다.
	void UpdateAuraLight(float DeltaSeconds);
	// 플레이어가 바라보는 방향과 관계없이 AuraLight를 플레이어 위치 + AuraLightOffset(월드 기준)에 둔다.
	void UpdateAuraLightLocation();
	// BeginPlay 시점에 BP에서 정한 AuraLight 밝기
	float AuraLightBaseIntensity = 0.0f;
	// 0이면 꺼짐, 1이면 원래 밝기
	float AuraLightAlpha = 1.0f;

	FTransform SafePointTransform;
	bool bHasSafePoint = false;
	// 커서의 위치 갱신
	void UpdateCursorTarget();
	// 투척 가능할 때만 조준 위젯을 띄우고, 마우스 위치로 옮긴다.
	void UpdateAimWidget();
	void SetAimWidgetVisible(bool bVisible);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> AimWidget;
	bool bAimWidgetVisible = false;
	// 조준 중 커서를 숨기기 전의 커서 모양
	EMouseCursor::Type SavedMouseCursor = EMouseCursor::Default;
	// 캐릭터가 커서를 바라보도록 업데이트하는 함수
	void UpdateCursorFacing(float DeltaSeconds);
	void SetCursorFacingActive(bool bActive);
	// 입력이 끊겨 중간에 멈춘 회전을 FacingTargetYaw까지 마저 돌린다.
	void SettleFacing(float DeltaSeconds);
	// FacingTargetYaw로 즉시 맞춘다. 공격처럼 방향이 바로 정해져야 할 때 사용한다.
	void SnapToFacingTarget();
	// A/D 입력이 들어오면 진행 중인 턴 클립을 끊는다.
	void CancelMoveTurn();
	// 공격 중(기본 콤보, 표창, 질풍참 등 Attacking 태그를 가진 어빌리티)인지. 끝날 때까지 이동·회전 입력을 막는 데 쓴다.
	bool IsAttacking() const;
	// 벽에 붙음·모서리 매달림·오르기·벽 타고 내려가기 중인지.
	bool IsOnWall() const;
	// 앉은 상태에서 일어설 공간이 있는지. 서 있으면 항상 true.
	bool HasRoomToStand() const;

	// 현재 제자리 턴이 회전 마무리(SettleFacing)로 시작되었는지. 커서 회전 종료 처리가 이 턴을 끊지 않게 한다.
	bool bMoveTurnActive = false;

    bool bParryMovementLocked = false;
    TWeakObjectPtr<UAnimInstance> ParryLockedAnimation;
    uint8 SavedParryRootMotionMode = 0;

    // bDebugInvincible에 맞춰 무적 태그를 붙이거나 뗀다. 태그는 개수로 관리되므로 붙인 횟수를 따로 기억한다.
    void SyncDebugInvincibleTag();
    bool bDebugInvincibleTagApplied = false;

	// 좌우 중 최종으로 바라봐야 할 방향(0 또는 180). 이동 입력, 커서, 벽 점프가 갱신한다.
	float FacingTargetYaw = 0.0f;

	FVector CursorWorldLocation = FVector::ZeroVector;
	// 유효한 커서 위치가 있는지 확인하는 변수
	bool bHasCursorTarget = false;
	// 현재 커서를 바라보는 기능이 켜져 있는지
	bool bCursorFacingActive = false;
	// 현재 카메라 탐색모드가 켜져 켜지 있는지
	bool bIsExploring = false;
	float CursorFacingYaw = 0.0f;

	bool bSavedOrientRotationToMovement = false;
	bool bSavedUseControllerDesiredRotation = false;
	bool bSavedUseControllerRotationYaw = false;

	// 플레이어를 전투 대상으로 삼은 적. 하나라도 있으면 Combat이다.
	TSet<TWeakObjectPtr<AEnemy>> EngagedEnemies;
	// PlayerCondition을 바꾸는 유일한 함수. 값이 바뀔 때만 알린다.
	void SetPlayerCondition(EPlayerCondition InPlayerCondition);

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

	// 이동 키를 뗐을 때 누르고 있던 방향 기록을 지운다
	void MoveReleased(const FInputActionValue& Value);

	// BP 컴파일에러 제거용 임시 함수
	void Drop(const FInputActionValue& Value) {}

	// BP 컴파일에러 제거용 임시 함수
	void DropStarted(const FInputActionValue& Value) {}

	// BP 컴파일에러 제거용 임시 함수
	void DropReleased(const FInputActionValue& Value) {}

	// 체력 어트리뷰트가 변경되었을 때 호출될 함수
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	// 사망 처리용 함수
	UFUNCTION(BlueprintCallable, Category = "Combat|Death")
	void Die();

	/**
	 * 죽으면 이전 맵으로 돌아가는 맵(AEclipseWorldSettings::bReturnToPreviousStageOnDeath, 보스 아레나)에서
	 * 사망 몽타주가 끝나기 이 시간(초) 전부터 화면을 검게 덮는다. 이전 맵을 불러오는 동안 죽은 맵 화면이 보이지 않게 한다
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Death", meta = (ClampMin = "0.0", Units = "s"))
	float DeathFadeOutTime = 0.5f;

	// 사망 몽타주 끝 무렵 화면을 검게 덮기 시작한다
	void StartDeathFadeOut(float Duration);

	FTimerHandle DeathFadeTimer;

	// ReceiveAssassination 안에서만 유효하다. Die가 사망 몽타주 대신 재생한다.
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> PendingAssassinationMontage;

	/** 좌우 이동 입력에 데드존과 최소 배율을 적용한다. 데드존 안이면 0을 반환한다. */
	float ApplyMoveInputDeadZone(float RawInput) const;

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoMove(float Forward);

	// BP 컴파일에러 제거용 임시 함수
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoDrop(float Value) {}


	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoJumpEnd();

	/** Handles interact inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoInteract();

	/** Handles Wall Climbing */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoWAllMove(float Up);



	// 카메라 이동 처리
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoCameraMoveStart();

	// 카메라 이동 처리
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoCameraMoveEnd();

	// 앉기 처리
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoCrouch();

	// 앉기 해제 처리
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoUnCrouch();

	// 질풍참 처리
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoSwiftStrike();

	/**
	 * DisableInput으로 입력을 끊었다가 되살린 뒤 호출한다.
	 * 끊긴 동안 뗀 키는 Completed가 오지 않으므로, 실제로 떼어진 키의 해제 처리(앉기·조준·점프·이동)를 대신 실행한다.
	 */
	void SyncHeldInputsAfterInputLock();

	/** 분신술 타게팅 상태 설정. 타게팅 중에는 카메라 시점을 유지하고, 종료되면 중앙으로 복귀한다. */
	void SetCloneTargeting(bool bTargeting);
	bool IsCloneTargeting() const { return bIsCloneTargeting; }

protected:

	/** 입력으로 카메라 시점 전환을 종료한다. 분신술 타게팅 중에는 무시한다. */
	void OnCameraMoveEndInput();

	/** 분신술 타게팅 중인지 여부 */
	bool bIsCloneTargeting = false;

	/** 앉기 키를 누르고 있는지 여부. 공중에서 누르면 착지할 때 앉는다. */
	bool bCrouchInputHeld = false;

	/** 앉은 채 모서리에서 떨어지는 중인지. 앉기 키를 떼거나 착지·안전 지점 복귀·벽 붙기 때 꺼진다 */
	bool bCrouchFalling = false;

	/** 안전 지점 복귀처럼 강제로 떨어뜨릴 때 앉은 낙하로 치지 않게 막는다 */
	bool bSuppressCrouchFall = false;

	/** 앉은 낙하 동안 앉은 걷기 속도로 바꾸기 전의 MaxWalkSpeed */
	float CrouchFallSavedMaxWalkSpeed = 0.0f;

	/** 앉은 낙하를 시작한다. 일어선 캡슐로 바꾸고 공중 이동 속도를 앉은 걷기 속도로 맞춘다 */
	void BeginCrouchFall();

	/** 앉은 낙하를 끝내고 공중 이동 속도를 되돌린다. 앉은 낙하 중이 아니면 아무것도 하지 않는다 */
	void EndCrouchFall();

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
	UFUNCTION(BlueprintPure, Category = "Side Scrolling")
	bool HasDoubleJumped() const { return false; }

	/** Returns true if the character has just wall jumped */
	UFUNCTION(BlueprintPure, Category = "Side Scrolling")
	bool HasWallJumped() const;
};
