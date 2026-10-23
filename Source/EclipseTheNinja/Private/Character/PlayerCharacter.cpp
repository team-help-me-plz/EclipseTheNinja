// Copyright Epic Games, Inc. All Rights Reserved.


#include "Character/PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Engine/World.h"
#include "SideScrollingInteractable.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "Character/PlayerCharacterController.h"
#include "Character/PlayerCharacterAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/StatAttributeSet.h"
#include "GAS/Ability/GABaseAttack.h"
#include "Components/BoxComponent.h"
#include "Engine/TargetPoint.h"
#include "Interface/Interactable.h"
#include "Component/AssassinationTargetingComponent.h"
#include "Component/AssassinateTargetComponent.h"
#include "GAS/Ability/GAAssassinate.h"
#include "Ability/GACloneTech.h"
#include "Component/InteractBoxComponent.h"
#include "GAS/Ability/GAWallAttach.h"
#include "GAS/Ability/GALedgeHang.h"
#include "GAS/Ability/GALedgeClimb.h"

APlayerCharacter::APlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    InteractComponent = CreateDefaultSubobject<UInteractBoxComponent>(TEXT("InteractComponent"));
    InteractComponent->SetupAttachment(RootComponent);
    InteractComponent->SetBoxExtent(InteractBoxSize);
    InteractComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractComponent->SetCollisionResponseToAllChannels(ECR_Overlap);
    InteractComponent->SetGenerateOverlapEvents(true);

	AssassinateTargetingComponent = CreateDefaultSubobject<UAssassinationTargetingComponent>(TEXT("AssassinateTargeting"));

	// 암살도 배열의 기본 항목으로 둔다. 기본 공격 항목은 부모 생성자에서 추가한다.
	FAbilityInfo& AssassinateInfo = AbilityClassArray.AddDefaulted_GetRef();
	AssassinateInfo.AbilityClass = UGAAssassinate::StaticClass();
	AssassinateInfo.InputID = EAbilityInputID::Assassinate;

	// create the camera component
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);

	Camera->SetRelativeLocationAndRotation(FVector(0.0f, 300.0f, 0.0f), FRotator(0.0f, -90.0f, 0.0f));

	// configure the collision capsule
	GetCapsuleComponent()->SetCapsuleSize(35.0f, 90.0f);

	// configure the character movement component
	GetCharacterMovement()->GravityScale = 1.75f;
	GetCharacterMovement()->MaxAcceleration = 1500.0f;
	GetCharacterMovement()->BrakingFrictionFactor = 1.0f;
	GetCharacterMovement()->bUseSeparateBrakingFriction = true;
	GetCharacterMovement()->Mass = 500.0f;

	GetCharacterMovement()->SetWalkableFloorAngle(75.0f);
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
	GetCharacterMovement()->bIgnoreBaseRotation = true;

	GetCharacterMovement()->PerchRadiusThreshold = 15.0f;
	GetCharacterMovement()->LedgeCheckThreshold = 6.0f;

	GetCharacterMovement()->JumpZVelocity = 750.0f;
	GetCharacterMovement()->AirControl = 1.0f;

	GetCharacterMovement()->RotationRate = FRotator(0.0f, 750.0f, 0.0f);
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// 일반 점프는 1회만 허용하고, 벽 점프는 LaunchCharacter로 처리한다.
	JumpMaxCount = 1;
}

void APlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    InteractComponent->SetBoxExtent(InteractBoxSize);
    InteractComponent->SetAbsolute(false, true, true);
    WarnIfAbilityMissing(EAbilityInputID::Assassinate);
    WarnIfAbilityMissing(EAbilityInputID::AerialAssassinate);
    WarnIfAbilityMissing(EAbilityInputID::BelowAssassinate);
    WarnIfAbilityMissing(EAbilityInputID::CloneTech);
    WarnIfAbilityMissing(EAbilityInputID::ThrowKunai);
    JumpMaxCount = 1; // 기존 BP에 남은 점프 횟수 설정도 1회로 맞춘다.
    FacingTargetYaw = GetActorForwardVector().X >= 0.0f ? 0.0f : 180.0f;

	// 체력 변화 델리게이트 바인딩
	HealthChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UStatAttributeSet::GetHealthAttribute())
		.AddUObject(this, &APlayerCharacter::OnHealthChanged);
}

void APlayerCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	SetCursorFacingActive(false);
	// clear the wall jump timer
	GetWorld()->GetTimerManager().ClearTimer(WallJumpTimer);
	Super::EndPlay(EndPlayReason);
}

void APlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsAssassinating()) return;

	UpdateCursorTarget();
	UpdateCursorFacing(DeltaSeconds);
}

void APlayerCharacter::UpdateCursorTarget()
{
	bHasCursorTarget = false;
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	// 우클릭을 놓으면 몸 회전을 끄고 고개는 정면으로 복귀한다.
	if (!PlayerController || !PlayerController->IsLocalPlayerController()
		|| !bIsExploring)
	{
		return;
	}

	FVector RayOrigin;
	FVector RayDirection;
	if (!PlayerController->DeprojectMousePositionToWorld(RayOrigin, RayDirection)
		|| FMath::IsNearlyZero(RayDirection.Y))
	{
		return;
	}

	// 커서 광선과 캐릭터가 서 있는 XZ 평면의 교점을 구한다.
	const float Distance = (GetActorLocation().Y - RayOrigin.Y) / RayDirection.Y;
	if (Distance < 0.0f)
	{
		return;
	}

	CursorWorldLocation = RayOrigin + RayDirection * Distance;
	bHasCursorTarget = true;
}

void APlayerCharacter::UpdateCursorFacing(float DeltaSeconds)
{

	if (IsValid(ASC))
	{
		// 매달려 있거나 모서리에서 내려가는 중에는 벽을 바라본 채로 둔다.
		if (ASC->HasMatchingGameplayTag(UGAWallAttach::GetWallAttachingTag())
			|| ASC->HasMatchingGameplayTag(UGALedgeHang::GetLedgeHangingTag()))
		{
			return;
		}
	}

	const bool bShouldFaceCursor = bFaceCursor && bHasCursorTarget;
	SetCursorFacingActive(bShouldFaceCursor);
	UPlayerCharacterAnimInstance* Animation = Cast<UPlayerCharacterAnimInstance>(GetMesh()->GetAnimInstance());
	if (bMoveTurnActive && !(Animation && Animation->IsTurningInPlace()))
	{
		bMoveTurnActive = false;
	}

	if (!bShouldFaceCursor || bHasWallJumped)
	{
		// A/D로 시작한 제자리 턴은 끝까지 재생한다.
		if (Animation && !bMoveTurnActive)
		{
			Animation->StopTurnInPlace();
		}
		// 우클릭 해제나 짧은 A/D 입력으로 회전이 중간에 끊겨도 좌우 방향까지 마저 돌린다.
		if (!bHasWallJumped)
		{
			SettleFacing(DeltaSeconds);
		}
		return;
	}

	const float HorizontalDistance = CursorWorldLocation.X - GetActorLocation().X;
	if (FMath::Abs(HorizontalDistance) > CursorFacingDeadZone)
	{
		CursorFacingYaw = HorizontalDistance > 0.0f ? 0.0f : 180.0f;
	}
	FacingTargetYaw = CursorFacingYaw;
	// 커서 회전이 켜지면 진행 중인 턴도 커서 기준으로 이어서 처리한다.
	bMoveTurnActive = false;

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const bool bCanContinueTurn = Movement->IsMovingOnGround()
		&& Movement->GetCurrentAcceleration().IsNearlyZero();
	const bool bCanTurnInPlace = bCanContinueTurn && GetVelocity().SizeSquared2D() < FMath::Square(20.0f);
	if (Animation && Animation->IsTurningInPlace())
	{
		if (bCanContinueTurn)
		{
			Animation->UpdateTurnInPlaceTarget(CursorFacingYaw);
			return; // 턴 클립이 회전하므로 코드 회전은 적용하지 않는다.
		}
		Animation->StopTurnInPlace();
	}

	const float RemainingYaw = FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, CursorFacingYaw));
	if (Animation && bCanTurnInPlace && RemainingYaw >= 175.0f
		&& Animation->TryStartTurnInPlace(CursorFacingYaw))
	{
		return;
	}

	// 중심 여유 구간에 들어와도 마지막 방향까지 부드럽게 회전한다.
	const FRotator TargetRotation(0.0f, CursorFacingYaw, 0.0f);
	const FRotator NewRotation = FMath::RInterpConstantTo(
		GetActorRotation(), TargetRotation, DeltaSeconds, FMath::Max(0.0f, CursorBodyTurnSpeed));
	SetActorRotation(NewRotation);
}

void APlayerCharacter::SettleFacing(float DeltaSeconds)
{
	// 이동 입력 중에는 CMC의 bOrientRotationToMovement가 회전을 맡는다.
	// 입력이 없으면 CMC는 회전을 멈추므로, 남은 회전을 여기서 이어서 돌린다.
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement->GetCurrentAcceleration().IsNearlyZero())
	{
		return;
	}

	// 제자리 턴 중에는 턴 클립의 루트 모션이 회전을 맡는다.
	UPlayerCharacterAnimInstance* Animation = Cast<UPlayerCharacterAnimInstance>(GetMesh()->GetAnimInstance());
	if (Animation && Animation->IsTurningInPlace())
	{
		return;
	}

	const float RemainingYaw = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, FacingTargetYaw);
	if (FMath::IsNearlyZero(RemainingYaw, 0.5f))
	{
		return;
	}

	// 땅에 서 있고 남은 회전이 크면, 턴 클립의 남은 구간을 재생해 발을 디디며 마무리한다.
	if (Animation && Movement->IsMovingOnGround() && FMath::Abs(RemainingYaw) >= MoveTurnClipMinYaw
		&& Animation->TryStartTurnInPlaceRemaining(FacingTargetYaw, RemainingYaw))
	{
		bMoveTurnActive = true;
		return;
	}

	// 클립으로 마무리할 수 없으면(작은 각도, 공중, 클립 회전 방향이 반대) 이동 회전과 같은 속도로 돌린다.
	const float TurnSpeed = Movement->RotationRate.Yaw > 0.0f ? Movement->RotationRate.Yaw : CursorBodyTurnSpeed;
	SetActorRotation(FMath::RInterpConstantTo(
		GetActorRotation(), FRotator(0.0f, FacingTargetYaw, 0.0f), DeltaSeconds, TurnSpeed));
}

void APlayerCharacter::SnapToFacingTarget()
{
	// 루트 모션을 끄지 않고 즉시 끊어야 턴 클립의 루트 회전이 공격 포즈에 남지 않는다.
	if (UPlayerCharacterAnimInstance* Animation = Cast<UPlayerCharacterAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		Animation->CancelTurnInPlace(0.0f);
	}
	bMoveTurnActive = false;
	SetActorRotation(FRotator(0.0f, FacingTargetYaw, 0.0f));
}

void APlayerCharacter::CancelMoveTurn()
{
	// A/D 입력이 다시 들어오면 턴 클립을 끊고 바로 이동한다. 서 있다가 움직일 때 지연이 없게 하기 위함이다.
	if (!bMoveTurnActive)
	{
		return;
	}
	if (UPlayerCharacterAnimInstance* Animation = Cast<UPlayerCharacterAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		Animation->CancelTurnInPlace(MoveTurnBlendOutTime);
	}
	bMoveTurnActive = false;
}

void APlayerCharacter::SetCursorFacingActive(bool bActive)
{
	if (bCursorFacingActive == bActive)
	{
		return;
	}

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (bActive)
	{
		CursorFacingYaw = GetActorForwardVector().X >= 0.0f ? 0.0f : 180.0f;
		// 커서 회전이 이동 및 컨트롤러 회전과 충돌하지 않게 한다.
		bSavedOrientRotationToMovement = Movement->bOrientRotationToMovement;
		bSavedUseControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
		bSavedUseControllerRotationYaw = bUseControllerRotationYaw;
		Movement->bOrientRotationToMovement = false;
		Movement->bUseControllerDesiredRotation = false;
		bUseControllerRotationYaw = false;
	}
	else
	{
		Movement->bOrientRotationToMovement = bSavedOrientRotationToMovement;
		Movement->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		bUseControllerRotationYaw = bSavedUseControllerRotationYaw;
	}
	bCursorFacingActive = bActive;
}

void APlayerCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Set up action bindings
    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (CloneTechAction)
        {
            EnhancedInputComponent->BindAction(CloneTechAction, ETriggerEvent::Started, this, &APlayerCharacter::DoCloneTech);
            EnhancedInputComponent->BindAction(CloneTechAction, ETriggerEvent::Completed, this, &APlayerCharacter::DoCloneTechEnd);
            EnhancedInputComponent->BindAction(CloneTechAction, ETriggerEvent::Canceled, this, &APlayerCharacter::DoCloneTechEnd);
        }
        if (CloneRecallAction)
        {
            EnhancedInputComponent->BindAction(CloneRecallAction, ETriggerEvent::Started, this, &APlayerCharacter::DoCloneRecall);
        }
        if (MoveScrollAction)
        {
            EnhancedInputComponent->BindAction(MoveScrollAction, ETriggerEvent::Triggered, this, &APlayerCharacter::DoMouseScroll);
        }

        // 카메라 이동 중 좌클릭은 모드를 유지한 채 표창을 던진다.
        EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Started, this, &APlayerCharacter::DoBasicAttack);

		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &APlayerCharacter::OnCameraMoveEndInput);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &APlayerCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &APlayerCharacter::DoJumpEnd);

		// Interacting
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &APlayerCharacter::OnCameraMoveEndInput);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &APlayerCharacter::DoInteract);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::OnCameraMoveEndInput);

		// Dropping from platform
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Started, this, &APlayerCharacter::DropStarted);
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Drop);
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Completed, this, &APlayerCharacter::DropReleased);

		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &APlayerCharacter::OnCameraMoveEndInput);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Canceled, this, &APlayerCharacter::OnCameraMoveEndInput);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &APlayerCharacter::DoCameraMoveStart);
	}
}

void APlayerCharacter::NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	// only apply push impulse if we're falling
	if (!GetCharacterMovement()->IsFalling())
	{
		return;
	}

	// ensure the colliding component is valid
	if (OtherComp)
	{
		// ensure the component is movable and simulating physics
		if (OtherComp->Mobility == EComponentMobility::Movable && OtherComp->IsSimulatingPhysics())
		{
			const FVector PushDir = FVector(ActionValueY > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);

			// push the component away
			OtherComp->AddImpulse(PushDir * JumpPushImpulse, NAME_None, true);
		}
	}
}

void APlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	bCanCoyoteJump = false;
}

void APlayerCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode /*= 0*/)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	// are we falling?
	if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Falling)
	{
		// save the game time when we started falling, so we can check it later for coyote time jumps
		LastFallTime = GetWorld()->GetTimeSeconds();
		bCanCoyoteJump = PrevMovementMode == MOVE_Walking && JumpCurrentCount == 0 && !bPressedJump;
	}
}

void APlayerCharacter::PossessedBy(AController* NewController)
{
	// 모서리 매달리기·올라가기는 BP 배열에 없으면 C++ 기본 클래스로 채운다. 이미 지정돼 있으면 그대로 둔다.
	MigrateLegacyAbility(EAbilityInputID::LedgeHang, nullptr, UGALedgeHang::StaticClass());
	MigrateLegacyAbility(EAbilityInputID::LedgeClimb, nullptr, UGALedgeClimb::StaticClass());

	// 어빌리티 배열은 부모의 PossessedBy에서 부여한다.
	Super::PossessedBy(NewController);

	if (IsValid(ASC))
	{

	}
}

void APlayerCharacter::PawnClientRestart()
{
	// 재빙의 시 GAS 정보를 갱신하고 로컬 공격 입력을 다시 연결한다.
	Super::PawnClientRestart();
	ASC->InitAbilityActorInfo(this, this);

}

void APlayerCharacter::UnPossessed()
{
	DoCameraMoveEnd();
	DoCloneTechEnd();
	bHasCursorTarget = false;
	SetCursorFacingActive(false);
	ASC->CancelAllAbilities();
	Super::UnPossessed();
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MoveVector = Value.Get<FVector2D>();

	// 위 입력(W, IA_Move의 X축)을 매달린 상태에서 누르면 모서리 위로 올라간다
	if (MoveVector.X > 0.5f && IsWallAttached())
	{
		TryActivateAbilityByInputID(EAbilityInputID::LedgeClimb);
	}

	// route the input
	DoMove(MoveVector.Y);
}

void APlayerCharacter::Drop(const FInputActionValue& Value)
{
	// route the input
	DoDrop(Value.Get<float>());
}

void APlayerCharacter::DropStarted(const FInputActionValue& Value)
{
	if (bIsDead || IsAssassinating()) return;
	if (!GetCharacterMovement()->IsMovingOnGround()) return;

	// 통과 가능한 발판 위에서는 기존 아래+점프 내려가기를 우선한다.
	const UPrimitiveComponent* Floor = GetCharacterMovement()->CurrentFloor.HitResult.GetComponent();
	if (IsValid(Floor) && Floor->GetCollisionObjectType() == SoftCollisionObjectType) return;

	// 모서리에 서 있으면 아래 벽으로 내려가 매달린다.
	TryActivateAbilityByInputID(EAbilityInputID::LedgeHang);
}

void APlayerCharacter::DropReleased(const FInputActionValue& Value)
{
	// reset the input
	DoDrop(0.0f);
}

void APlayerCharacter::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	// 체력이 0이면 사망하고, 살아 있는 상태의 체력 감소는 피격으로 처리한다.
	if (bIsDead)
	{
		return;
	}
	if (Data.NewValue <= 0.0f)
	{
		Die();
		return;
	}
	if (Data.NewValue < Data.OldValue)
	{
		PlayHitReaction();
	}
}

void APlayerCharacter::Die()
{
	// 사망은 한 번만 처리하고 공격, AI, 이동과 충돌을 정리한다.
	if (bIsDead)
	{
		return;
	}

	//사망 처리
	bIsDead = true;
	DoCameraMoveEnd();
	DoCloneTechEnd();
	ASC->CancelAllAbilities();

	// 정지 후 움직이지 못하게 세팅
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// 사망 몽타주 재생
	float MontageDuration = 0.0f;
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (DeathMontage)
		{
			MontageDuration = AnimInstance->Montage_Play(DeathMontage, 1.0f, EMontagePlayReturnType::Duration);
			if (MontageDuration > 0.0f)
			{
				FOnMontageBlendingOutStarted BlendOutDelegate;
				BlendOutDelegate.BindUObject(this, &APlayerCharacter::OnDeathMontageBlendingOut);
				AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, DeathMontage);
			}
		}
	}

	if (MontageDuration <= 0.0f)
	{
		// A missing/incompatible montage must not leave an immortal corpse.
		GetMesh()->bPauseAnims = true;
		UE_LOG(LogTemp, Warning, TEXT("%s: Death montage was not set or could not be played."), *GetName());
	}
	// SetLifeSpan(0) cancels destruction, so keep a small positive minimum.
	SetLifeSpan(FMath::Max(0.01f, MontageDuration));
}

void APlayerCharacter::DoBasicAttack()
{
	if (bIsDead) return;
	if (!ASC) return;
	if (IsAssassinating()) return;
	if (bIsExploring)
	{
		DoThrowKunai();
		return;
	}

	if (!FindAbilitySpecByInputID(EAbilityInputID::BasicAttack)) return;
	if (!BasicAttackMontage) return;

	// 첫 공격 전에만 방향을 확정하고, 콤보 추가 입력은 기존 공격 방향을 유지한다.
	if (!ASC->HasMatchingGameplayTag(UGABaseAttack::GetAttackingTag()))
	{
		SnapToFacingTarget();
	}

	Super::DoBasicAttack();
}

void APlayerCharacter::DoAssassinate()
{
	if (bIsDead) return;
	if (!IsValid(ASC)) return;
	if (IsAssassinating()) return;
	if (!IsValid(AssassinateTargetingComponent)) return;

	UAssassinateTargetComponent* Target = AssassinateTargetingComponent->GetCurrentTarget();
	if (!IsValid(Target)) return;
	if (!Target->CanAssassinate()) return;

	// 타겟을 고를 때 정한 방식(땅: 등 뒤, 공중: 머리 위)에 맞는 어빌리티를 발동한다.
	const EAssassinationType Type = AssassinateTargetingComponent->GetCurrentTargetType();
	if (Type == EAssassinationType::Ground)
	{
		TryActivateAbilityByInputID(EAbilityInputID::Assassinate);
	}
	else if (Type == EAssassinationType::Aerial)
	{
		TryActivateAbilityByInputID(EAbilityInputID::AerialAssassinate);
	}
	else if (Type == EAssassinationType::Below)
	{
		TryActivateAbilityByInputID(EAbilityInputID::BelowAssassinate);
	}
}

bool APlayerCharacter::IsAssassinating() const
{
	if (!IsValid(ASC)) return false;
	return ASC->HasMatchingGameplayTag(UGAAssassinate::GetAssassinatingTag());
}

bool APlayerCharacter::IsWallAttached() const
{
	if (!IsValid(ASC)) return false;
	return ASC->HasMatchingGameplayTag(UGAWallAttach::GetWallAttachingTag());
}

void APlayerCharacter::SetAssassinationFacing(float Yaw)
{
	SetCursorFacingActive(false);
	FacingTargetYaw = Yaw;
	SnapToFacingTarget();
}

void APlayerCharacter::DoThrowKunai()
{
    if (!CanThrowKunai() || !ASC)
    {
        return;
    }

    // 투척 어빌리티가 지급되지 않았으면 몸 방향도 바꾸지 않는다.
    if (!FindAbilitySpecByInputID(EAbilityInputID::ThrowKunai))
    {
        return;
    }

    // 좌클릭 입력이 Tick보다 먼저 와도 현재 커서로 조준한다.
    UpdateCursorTarget();
    if (bHasCursorTarget)
    {
        const float DirectionX = CursorWorldLocation.X - GetActorLocation().X;
        if (FMath::Abs(DirectionX) > CursorFacingDeadZone)
        {
            FacingTargetYaw = DirectionX > 0.0f ? 0.0f : 180.0f;
        }
    }

    SnapToFacingTarget();

    TryActivateAbilityByInputID(EAbilityInputID::ThrowKunai);
}

void APlayerCharacter::DoMove(float Forward)
{
	if (IsAssassinating()) return;

	// 매달린 동안(모서리에서 내려가는 중 포함) 일반 이동 입력이 목표 방향을 바꾸지 않게 한다.
	if (IsValid(ASC))
	{
		if (ASC->HasMatchingGameplayTag(UGAWallAttach::GetWallAttachingTag())
			|| ASC->HasMatchingGameplayTag(UGALedgeHang::GetLedgeHangingTag()))
		{
			return;
		}
	}

	// is movement temporarily disabled after wall jumping?
	if (!bHasWallJumped)
	{
		// save the movement values
		ActionValueY = Forward;

		// 입력이 짧게 끊겨도 이 방향까지 회전을 마무리한다.
		if (!FMath::IsNearlyZero(Forward))
		{
			FacingTargetYaw = Forward > 0.0f ? 0.0f : 180.0f;

			// 이동 입력은 지연 없이 바로 적용한다. 진행 중인 턴 클립은 끊는다.
			CancelMoveTurn();
		}

		// figure out the movement direction
		const FVector MoveDir = FVector(1.0f, Forward > 0.0f ? 0.1f : -0.1f, 0.0f);

		// apply the movement input
		AddMovementInput(MoveDir, Forward);
	}
}

void APlayerCharacter::DoDrop(float Value)
{
	if (IsAssassinating()) return;

	// save the movement value
	DropValue = Value;
}

void APlayerCharacter::DoJumpStart()
{
	if (IsAssassinating()) return;

	// 입력 상태에 따라 점프 또는 발판 내려가기를 선택한다.
	MultiJump();
}

void APlayerCharacter::DoJumpEnd()
{
	StopJumping();
}

void APlayerCharacter::DoInteract()
{
	if (bIsDead) return;
	if (IsAssassinating()) return;

	// 상호작용보다 암살을 우선하고, 실패하더라도 같은 입력으로 상호작용하지 않는다.
	if (IsValid(AssassinateTargetingComponent))
	{
		UAssassinateTargetComponent* Target = AssassinateTargetingComponent->GetCurrentTarget();
		if (IsValid(Target) && Target->CanAssassinate())
		{
			DoAssassinate();
			return;
		}
	}

    this->InteractComponent->TryInteract();
    //// Clone-tech interactables (levers, clone launch, traps) use a separate interface.
    //TArray<AActor*> Candidates;
    //InteractBox->GetOverlappingActors(Candidates);
    //for (AActor* Candidate : Candidates)
    //{
    //    if (IsValid(Candidate) && Candidate != this && Candidate->Implements<UInteractable>()
    //        && IInteractable::Execute_bIsInteractReady(Candidate))
    //    {
    //        IInteractable::Execute_Interact(Candidate, this);
    //        return;
    //    }
    //}

    //// do a sphere trace to look for interactive objects
    //FHitResult OutHit;

    //const FVector Start = GetActorLocation();
    //const FVector End = Start + FVector(100.0f, 0.0f, 0.0f);

    //FCollisionShape ColSphere;
    //ColSphere.SetSphere(InteractionRadius);

    //FCollisionObjectQueryParams ObjectParams;
    //ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
    //ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

    //FCollisionQueryParams QueryParams;
    //QueryParams.AddIgnoredActor(this);

    //if (GetWorld()->SweepSingleByObjectType(OutHit, Start, End, FQuat::Identity, ObjectParams, ColSphere, QueryParams))
    //{
    //    // have we hit an interactable?
    //    if (ISideScrollingInteractable* Interactable = Cast<ISideScrollingInteractable>(OutHit.GetActor()))
    //    {
    //        // interact
    //        Interactable->Interaction(this);
    //    }
    //}
}

void APlayerCharacter::DoCameraMoveStart()
{
	APlayerCharacterController* APC = Cast<APlayerCharacterController>(GetController());
	if (!APC)
	{
		UE_LOG(LogTemp, Log, TEXT("[APlayerCharacter::DoCameraMoveStart] : APC Was nullptr"));
		return;
	}

	APC->StartCameraMove();
	bIsExploring = true;
}

void APlayerCharacter::DoCameraMoveEnd()
{
	APlayerCharacterController* APC = Cast<APlayerCharacterController>(GetController());
	if (!APC)
	{
		UE_LOG(LogTemp, Log, TEXT("[APlayerCharacter::DoCameraMoveStart] : APC Was nullptr"));
		return;
	}

	APC->StopCameraMove();
	bIsExploring = false;
}

void APlayerCharacter::OnCameraMoveEndInput()
{
	// 분신술 타게팅 IMC가 추가되면 눌려 있던 우클릭이 Completed 처리되므로, 타게팅 중에는 시점을 유지한다.
	if (bIsCloneTargeting)
	{
		return;
	}
	DoCameraMoveEnd();
}

void APlayerCharacter::SetCloneTargeting(bool bTargeting)
{
	if (bIsCloneTargeting == bTargeting)
	{
		return;
	}
	bIsCloneTargeting = bTargeting;
	// 분신 소환 또는 취소 시 카메라를 중앙으로 되돌린다.
	if (!bTargeting && bIsExploring)
	{
		DoCameraMoveEnd();
	}
}

void APlayerCharacter::MultiJump()
{
	// 벽에 매달린 경우 벽점프
	if (IsValid(ASC))
	{
		// 태그를 통해서 벽에 붙어있는지 확인
		if (ASC->HasMatchingGameplayTag(UGAWallAttach::GetWallAttachingTag()))
		{
			// 이미 점프중이면 종료
			if (bHasWallJumped) return;

			// 벽에 매달리는 어빌리티가 부여되어 있는지 확인
			FGameplayAbilitySpec* WallAttachSpec = FindAbilitySpecByInputID(EAbilityInputID::WallAttach);
			if (!WallAttachSpec) return;

			// 벽에 매달리기 비활성화
			ASC->CancelAbilityHandle(WallAttachSpec->Handle);

			// 매달리기가 취소되지 않았다면 점프하지 않는다.
			if (ASC->HasMatchingGameplayTag(UGAWallAttach::GetWallAttachingTag()))
			{
				return;
			}

			const FVector JumpDirection = GetActorForwardVector().X >= 0.0f ? -FVector::ForwardVector : FVector::ForwardVector;
			DropValue = 0.0f;
			bCanCoyoteJump = false;

			// 벽에서 튕겨 나가는 방향을 바라본다.
			FacingTargetYaw = JumpDirection.X > 0.0f ? 0.0f : 180.0f;
			SetActorRotation(FRotator(0.0f, FacingTargetYaw, 0.0f));

			FVector WallJumpImpulse = JumpDirection * WallJumpHorizontalImpulse;
			WallJumpImpulse.Z = GetCharacterMovement()->JumpZVelocity * WallJumpVerticalMultiplier;

			LaunchCharacter(WallJumpImpulse, true, true);
			// 벽점프 직후 이동 입력과 재부착을 잠시 제한한다.
			bHasWallJumped = true;

			GetWorld()->GetTimerManager().SetTimer(
				WallJumpTimer,
				this,
				&APlayerCharacter::ResetWallJump,
				DelayBetweenWallJumps,
				false);

			return;
		}
	}

	// 아래 방향 입력 중에는 점프보다 발판 내려가기를 우선한다.
	if (DropValue > 0.0f)
	{
		CheckForSoftCollision();
		return;
	}

	// reset the drop value
	DropValue = 0.0f;

	// 지상에서는 일반 점프를 실행한다.
	if (GetCharacterMovement()->IsMovingOnGround())
	{
		Jump();
		return;
	}

	if (!GetCharacterMovement()->IsFalling()) return;
	if (bHasWallJumped) return;

	if (IsValid(ASC))
	{
		// 매달리기가 부여되지 않았으면 기존 코요테 점프 검사를 이어간다.
		FGameplayAbilitySpec* WallAttachSpec = FindAbilitySpecByInputID(EAbilityInputID::WallAttach);
		if (WallAttachSpec)
		{
			if (ASC->TryActivateAbility(WallAttachSpec->Handle))
			{
				bCanCoyoteJump = false;
				return;
			}
		}
	}

	// 매달리기에 실패하거나 ASC가 없으면 코요테 점프를 검사한다.
	if (!bCanCoyoteJump) return;
	if (bPressedJump) return;
	if (GetWorld()->GetTimeSeconds() - LastFallTime >= MaxCoyoteTime) return;

	bCanCoyoteJump = false;

	LaunchCharacter(FVector(0.0f, 0.0f, GetCharacterMovement()->JumpZVelocity), false, true);
}


void APlayerCharacter::CheckForSoftCollision()
{
	// reset the drop value
	DropValue = 0.0f;

	// trace down 
	FHitResult OutHit;

	const FVector Start = GetActorLocation();
	const FVector End = Start + (FVector::DownVector * SoftCollisionTraceDistance);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(SoftCollisionObjectType);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByObjectType(OutHit, Start, End, ObjectParams, QueryParams);

	// did we hit a soft floor?
	if (OutHit.GetActor())
	{
		// drop through the floor
		SetSoftCollision(true);
	}
}

void APlayerCharacter::ResetWallJump()
{
	// reset the wall jump flag
	bHasWallJumped = false;
}

void APlayerCharacter::SetSoftCollision(bool bEnabled)
{
	// enable or disable collision response to the soft collision channel
	GetCapsuleComponent()->SetCollisionResponseToChannel(SoftCollisionObjectType, bEnabled ? ECR_Ignore : ECR_Block);
}

bool APlayerCharacter::HasWallJumped() const
{
	return bHasWallJumped;
}

void APlayerCharacter::MigrateLegacyAbilities()
{
	Super::MigrateLegacyAbilities();

	// 예전에는 BP 값이 없으면 C++ 기본 암살 클래스로 부여했으므로 같은 규칙으로 옮긴다.
	MigrateLegacyAbility(EAbilityInputID::Assassinate, AssassinationAbilityClass_DEPRECATED, UGAAssassinate::StaticClass());
	AssassinationAbilityClass_DEPRECATED = nullptr;
}

void APlayerCharacter::DoCloneTech()
{
	if (IsAssassinating()) return;

	if (bIsDead || !ASC) return;
	//DoCameraMoveEnd();
	ASC->AbilityLocalInputPressed(ToAbilityInputID(EAbilityInputID::CloneTech));
}

void APlayerCharacter::DoCloneTechEnd()
{
	if (ASC) ASC->AbilityLocalInputReleased(ToAbilityInputID(EAbilityInputID::CloneTech));
}

void APlayerCharacter::DoCloneRecall()
{
    if (bIsDead) return;
    if (!IsValid(ASC)) return;

    /* TODO: Player 상태가 발각됐거나 전투중이면 바로 리턴하는 코드가 필요해용
    */
    // 기본 어빌리티와 같은 입력 ID 규칙으로 분신 회수 대상을 찾는다.
    FGameplayAbilitySpec* Spec = FindAbilitySpecByInputID(EAbilityInputID::CloneTech);
    // 분신이 없어지면 UGACloneTech는 EndAbility가 호출되므로 Activate 검사는 하지 않는다
    if (!Spec)
    {
        return;
    }
    // CloneAbility 얻기
    UGACloneTech* CloneAbility = Cast<UGACloneTech>(Spec->GetPrimaryInstance());

    if (CloneAbility)
    {
        // Recall하기
        CloneAbility->OnRecallClone();
    }
}

void APlayerCharacter::DoMouseScroll(const FInputActionValue& InValue)
{
    const float	Axis = InValue.Get<float>();
    int32		Direction = 0;

    if (Axis < 0)
    {
        Direction--;
    }
    else if (Axis > 0)
    {
        Direction++;
    }
    this->InteractComponent->OnScrollInput(Direction);
    //if (Axis > 0)
    //{
    //	this->InteractComponent->
    //}
}

void APlayerCharacter::RegisterSafePoint_Implementation(const ATargetPoint* InSafePoint)
{
	if (!IsValid(InSafePoint)) return;
	SafePointTransform = InSafePoint->GetActorTransform();
	bHasSafePoint = true;
}

void APlayerCharacter::ReturnToSafePoint_Implementation()
{
	if (!bHasSafePoint || bIsDead) return;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetPlaneConstraintOrigin(SafePointTransform.GetLocation());
	SetActorLocationAndRotation(SafePointTransform.GetLocation(), SafePointTransform.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
	FacingTargetYaw = GetActorForwardVector().X >= 0.f ? 0.f : 180.f;
}
