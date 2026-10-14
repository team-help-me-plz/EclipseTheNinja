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
#include "InputCoreTypes.h"
#include "GAS/StatAttributeSet.h"

APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

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
	JumpMaxCount = 1; // 기존 BP에 남은 점프 횟수 설정도 1회로 맞춘다.

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
	UpdateCursorTarget();
	UpdateCursorFacing(DeltaSeconds);
}

void APlayerCharacter::UpdateCursorTarget()
{
	bHasCursorTarget = false;
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	// 우클릭을 놓으면 몸 회전을 끄고 고개는 정면으로 복귀한다.
	if (!PlayerController || !PlayerController->IsLocalPlayerController()
		|| !PlayerController->IsInputKeyDown(EKeys::RightMouseButton))
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
	const bool bShouldFaceCursor = bFaceCursor && bHasCursorTarget;
	SetCursorFacingActive(bShouldFaceCursor);
	UPlayerCharacterAnimInstance* Animation = Cast<UPlayerCharacterAnimInstance>(GetMesh()->GetAnimInstance());
	if (!bShouldFaceCursor || bHasWallJumped)
	{
		if (Animation)
		{
			Animation->StopTurnInPlace();
		}
		return;
	}

	const float HorizontalDistance = CursorWorldLocation.X - GetActorLocation().X;
	if (FMath::Abs(HorizontalDistance) > CursorFacingDeadZone)
	{
		CursorFacingYaw = HorizontalDistance > 0.0f ? 0.0f : 180.0f;
	}

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
		EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Started, this, &APlayerCharacter::DoBasicAttack);

		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &APlayerCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &APlayerCharacter::DoJumpEnd);

		// Interacting
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Triggered, this, &APlayerCharacter::DoInteract);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);

		// Dropping from platform
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Drop);
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Completed, this, &APlayerCharacter::DropReleased);

		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &APlayerCharacter::DoCameraMoveStart);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &APlayerCharacter::DoCameraMoveEnd);
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
	Super::PossessedBy(NewController);

	if (IsValid(ASC))
	{
		//TODO 테스트용 함수로 추후에 기본 어빌리티 주는 함수 들어오면 삭제
		GiveTestAbility();
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
	bHasCursorTarget = false;
	SetCursorFacingActive(false);
	ASC->CancelAllAbilities();
	Super::UnPossessed();
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MoveVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MoveVector.Y);
}

void APlayerCharacter::Drop(const FInputActionValue& Value)
{
	// route the input
	DoDrop(Value.Get<float>());
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
	// 우클릭 중에는 새 공격과 콤보 입력을 모두 막는다.
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController && PlayerController->IsInputKeyDown(EKeys::RightMouseButton))
	{
		return;
	}

	Super::DoBasicAttack();
}

void APlayerCharacter::DoMove(float Forward)
{
	// is movement temporarily disabled after wall jumping?
	if (!bHasWallJumped)
	{
		// save the movement values
		ActionValueY = Forward;

		// figure out the movement direction
		const FVector MoveDir = FVector(1.0f, Forward > 0.0f ? 0.1f : -0.1f, 0.0f);

		// apply the movement input
		AddMovementInput(MoveDir, Forward);
	}
}

void APlayerCharacter::DoDrop(float Value)
{
	// save the movement value
	DropValue = Value;
}

void APlayerCharacter::DoJumpStart()
{
	// 입력 상태에 따라 점프 또는 발판 내려가기를 선택한다.
	MultiJump();
}

void APlayerCharacter::DoJumpEnd()
{
	StopJumping();
}

void APlayerCharacter::DoInteract()
{
	// do a sphere trace to look for interactive objects
	FHitResult OutHit;

	const FVector Start = GetActorLocation();
	const FVector End = Start + FVector(100.0f, 0.0f, 0.0f);

	FCollisionShape ColSphere;
	ColSphere.SetSphere(InteractionRadius);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->SweepSingleByObjectType(OutHit, Start, End, FQuat::Identity, ObjectParams, ColSphere, QueryParams))
	{
		// have we hit an interactable?
		if (ISideScrollingInteractable* Interactable = Cast<ISideScrollingInteractable>(OutHit.GetActor()))
		{
			// interact
			Interactable->Interaction(this);
		}
	}
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
}

void APlayerCharacter::MultiJump()
{
	// 아래 방향 입력 중에는 점프보다 발판 내려가기를 우선한다.
	if (DropValue > 0.0f)
	{
		CheckForSoftCollision();
		return;
	}

	// reset the drop value
	DropValue = 0.0f;

	// 지상에서는 일반 점프를 실행한다.
	if (!GetCharacterMovement()->IsFalling())
	{
		Jump();
		return;
	}

	// 공중에서 좌우 입력이 있으면 벽 점프를 먼저 시도한다.
	if (!bHasWallJumped && !FMath::IsNearlyZero(ActionValueY))
	{
		// trace ahead of the character for walls
		FHitResult OutHit;

		const FVector Start = GetActorLocation();
		const FVector End = Start + (FVector(ActionValueY > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f) * WallJumpTraceDistance);

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);

		GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, QueryParams);

		if (OutHit.bBlockingHit)
		{
			bCanCoyoteJump = false;
			// rotate to the bounce direction
			const FRotator BounceRot = UKismetMathLibrary::MakeRotFromX(OutHit.ImpactNormal);
			SetActorRotation(FRotator(0.0f, BounceRot.Yaw, 0.0f));

			// calculate the impulse vector
			FVector WallJumpImpulse = OutHit.ImpactNormal * WallJumpHorizontalImpulse;
			WallJumpImpulse.Z = GetCharacterMovement()->JumpZVelocity * WallJumpVerticalMultiplier;

			// launch the character away from the wall
			LaunchCharacter(WallJumpImpulse, true, true);

			// enable wall jump lockout for a bit
			bHasWallJumped = true;

			// schedule wall jump lockout reset
			GetWorld()->GetTimerManager().SetTimer(WallJumpTimer, this, &APlayerCharacter::ResetWallJump, DelayBetweenWallJumps, false);

			return;
		}
	}



	// 발판에서 걸어 나온 직후에만 짧은 점프 유예를 허용한다. 2단 점프는 없다.
	if (!bHasWallJumped && bCanCoyoteJump && !bPressedJump
		&& GetWorld()->GetTimeSeconds() - LastFallTime < MaxCoyoteTime)
	{
		bCanCoyoteJump = false;
		LaunchCharacter(FVector(0.0f, 0.0f, GetCharacterMovement()->JumpZVelocity), false, true);
	}
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


/*============================================================
여기 있는 코드는 단순 테스트용 실제 빌드에서는 사용하지 마시오
=============================================================*/
void APlayerCharacter::GiveTestAbility()
{
	if (!HasAuthority() || !ASC || !TestAbilityClass || ASC->FindAbilitySpecFromClass(TestAbilityClass)) return;
	FGameplayAbilitySpec Spec(TestAbilityClass, TestAbilityLevel);
	ASC->GiveAbility(Spec);
}
