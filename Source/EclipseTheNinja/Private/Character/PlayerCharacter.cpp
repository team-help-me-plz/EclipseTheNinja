// Copyright Epic Games, Inc. All Rights Reserved.


#include "Character/PlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Save/GameSaveSubsystem.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "GAS/Ability/GAParry.h"
#include "GAS/Tags/EclipseGameplayTags.h"
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
#include "GAS/Effects/GEDefaultStats.h"
#include "GAS/Ability/GABaseAttack.h"
#include "Components/BoxComponent.h"
#include "Engine/TargetPoint.h"
#include "Interface/Interactable.h"
#include "Component/AssassinationTargetingComponent.h"
#include "Component/AssassinateTargetComponent.h"
#include "GAS/Ability/GAAssassinate.h"
#include "GAS/Ability/GACloneTech.h"
#include "Component/InteractBoxComponent.h"
#include "GAS/Ability/GAWallAttach.h"
#include "Subsystem/UIMessageSubsystem.h"

#include "GAS/Ability/GALedgeHang.h"
#include "GAS/Ability/GALedgeClimb.h"
#include "Component/NoiseComponent.h"
#include "Noise/NoiseTypes.h"
#include "GAS/Ability/GAWallSlide.h"
#include "Character/Enemy.h"
#include "Component/HidingComponent.h"
#include "Minimap/MinimapComponent.h"
#include "Util/EclipseCollisionChannels.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"

APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	InteractComponent = CreateDefaultSubobject<UInteractBoxComponent>(TEXT("InteractComponent"));
	DefaultStatsEffect = UGEPlayerDefaultStats::StaticClass();
	InteractComponent->SetupAttachment(RootComponent);
	InteractComponent->SetBoxExtent(InteractBoxSize);
	InteractComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractComponent->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractComponent->SetGenerateOverlapEvents(true);

	AssassinateTargetingComponent = CreateDefaultSubobject<UAssassinationTargetingComponent>(TEXT("AssassinateTargeting"));

	NoiseComponent = CreateDefaultSubobject<UNoiseComponent>(TEXT("NoiseComponent"));

	HidingComponent = CreateDefaultSubobject<UHidingComponent>(TEXT("HidingComponent"));

	MinimapComponent = CreateDefaultSubobject<UMinimapComponent>(TEXT("MinimapComponent"));

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

	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 250.0f;

	GetCharacterMovement()->RotationRate = FRotator(0.0f, 750.0f, 0.0f);
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// 일반 점프는 1회만 허용하고, 벽 점프는 LaunchCharacter로 처리한다.
	JumpMaxCount = 1;

	// 우클릭 조준 위젯 기본값. BP에서 다른 위젯으로 바꿀 수 있다.
	static ConstructorHelpers::FClassFinder<UUserWidget> AimWidgetFinder(TEXT("/Game/Widgets/WBP_AimWidget"));
	if (AimWidgetFinder.Succeeded())
	{
		AimWidgetClass = AimWidgetFinder.Class;
	}
}

void APlayerCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	PlayActionSound(EPlayerActionSound::Jump);

	this->NoiseComponent->EmitNoise(ENoiseType::Jump);
}

void APlayerCharacter::BeginPlay()
{
  // 부모의 GAS 초기화 전에 기본 패링 항목을 보완한다. BP에서 지정한 클래스가 우선한다.
  MigrateLegacyAbility(EAbilityInputID::Parry, nullptr, UGAParry::StaticClass());
  Super::BeginPlay();
  InteractComponent->SetBoxExtent(InteractBoxSize);
  InteractComponent->SetAbsolute(false, true, true);
  WarnIfAbilityMissing(EAbilityInputID::Assassinate);
  WarnIfAbilityMissing(EAbilityInputID::AerialAssassinate);
  WarnIfAbilityMissing(EAbilityInputID::BelowAssassinate);
  WarnIfAbilityMissing(EAbilityInputID::CloneTech);
  WarnIfAbilityMissing(EAbilityInputID::ThrowKunai);
  WarnIfAbilityMissing(EAbilityInputID::ElementalFire);
  JumpMaxCount = 1; // 기존 BP에 남은 점프 횟수 설정도 1회로 맞춘다.

  // 플레이어는 몬스터를 통과한다. Blueprint에 저장된 캡슐 충돌 설정보다 우선하도록 런타임에 지정한다.
  GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Monster, ECR_Ignore);

  // 메시가 몬스터 캡슐을 막으면 몬스터가 플레이어 메시를 밟고 튄다.
  GetMesh()->SetCollisionResponseToChannel(ECC_Monster, ECR_Ignore);
  FacingTargetYaw = GetActorForwardVector().X >= 0.0f ? 0.0f : 180.0f;

	// 체력 변화 델리게이트 바인딩
	HealthChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UStatAttributeSet::GetHealthAttribute())
		.AddUObject(this, &APlayerCharacter::OnHealthChanged);

	// 패링 판정 중 들어온 적의 공격은 피해 대신 패링(적 경직/투사체 튕겨내기)으로 처리한다.
	ASC->GameplayEffectApplicationQueries.Add(FGameplayEffectApplicationQuery::CreateWeakLambda(this,
		[this](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec& Spec)
		{
			if (UGAParry::TryParryIncomingEffect(ASC, Spec)) return false;
			return true;
		}));
	SyncDebugInvincibleTag();

	// TOOD 변수로 만들기
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxFlySpeed = 121.f;
	Movement->BrakingDecelerationFlying = Movement->BrakingDecelerationWalking;

    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        GetGameInstance()->GetSubsystem<UGameSaveSubsystem>()->PlayerReady(this);
    }));

}

void APlayerCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	SetCursorFacingActive(false);
	SetAimWidgetVisible(false);
	if (AimWidget)
	{
		AimWidget->RemoveFromParent();
		AimWidget = nullptr;
	}
	// clear the wall jump timer
	GetWorld()->GetTimerManager().ClearTimer(WallJumpTimer);
	Super::EndPlay(EndPlayReason);
}

void APlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateMovementSound(DeltaSeconds);
	// 아래의 조기 반환(공격 중 등)과 관계없이 조준 위젯은 매 프레임 갱신한다.
	UpdateAimWidget();
	// 상호작용 키는 암살이 우선이므로, 암살이 가능한 동안에는 상호작용 표시를 숨겨 헷갈리지 않게 한다.
	if (IsValid(InteractComponent))
	{
		InteractComponent->SetIndicatorSuppressed(CanAssassinateCurrentTarget());
	}
	if (IsParrying()) return;
	// 사망 연출과 공격(콤보, 표창, 질풍참 등) 중에는 커서·이동 방향으로 몸을 돌리지 않는다.
	if (bIsDead || IsAssassinating() || IsHiding() || IsAttacking()) return;
	// 벤트 통과처럼 이동을 멈춰 둔 연출(MOVE_None) 중에도 커서로 몸을 돌리지 않는다.
	if (GetCharacterMovement()->MovementMode == MOVE_None) return;
	// 상호작용 몽타주 중에는 상호작용 대상을 바라본 채로 둔다.
	if (IsValid(InteractComponent) && InteractComponent->IsPlayingInteractMontage()) return;

	UpdateCursorTarget();
	UpdateCursorFacing(DeltaSeconds);
}

void APlayerCharacter::PlayCharacterSound(USoundBase* Sound, float Volume) const
{
	if (Sound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), Volume);
	}
}

void APlayerCharacter::PlayActionSound(EPlayerActionSound Action, float Volume) const
{
	if (const TObjectPtr<USoundBase>* Sound = ActionSounds.Find(Action))
	{
		PlayCharacterSound(Sound->Get(), Volume);
	}
}

void APlayerCharacter::PlaySwordAttackSound(int32 ComboIndex, bool bFire) const
{
	const auto& Sounds = bFire ? FireAttackSounds : SwordAttackSounds;
	if (Sounds.IsValidIndex(ComboIndex - 1))
	{
		PlayCharacterSound(Sounds[ComboIndex - 1]);
	}
}

void APlayerCharacter::PlayRandomCharacterSound(const TArray<TObjectPtr<USoundBase>>& Sounds) const
{
	if (!Sounds.IsEmpty())
	{
		PlayCharacterSound(Sounds[FMath::RandRange(0, Sounds.Num() - 1)]);
	}
}

void APlayerCharacter::PlayAssassinationWithdrawalSound() const
{
	PlayRandomCharacterSound(AssassinationSounds);
}

void APlayerCharacter::UpdateMovementSound(float DeltaSeconds)
{
	const bool bOnWall = IsWallAttached();
	const float Speed = bOnWall ? FMath::Abs(GetVelocity().Z) : GetVelocity().Size2D();
	if (bIsDead || IsAssassinating() || IsAttacking() || IsHiding() || Speed < 10.0f
		|| (!bOnWall && !GetCharacterMovement()->IsMovingOnGround()))
	{
		DistanceSinceMovementSound = 0.0f;
		PreviousMovementSounds = nullptr;
		return;
	}

	const auto& Sounds = bOnWall ? WallClimbSounds : (bIsCrouched ? CrouchWalkSounds : WalkSounds);
	if (PreviousMovementSounds != &Sounds)
	{
		DistanceSinceMovementSound = 0.0f;
		PreviousMovementSounds = &Sounds;
	}

	const float StepDistance = bOnWall ? 75.0f : (bIsCrouched ? 80.0f : 130.0f);
	DistanceSinceMovementSound += Speed * DeltaSeconds;
	if (DistanceSinceMovementSound >= StepDistance)
	{
		DistanceSinceMovementSound = FMath::Fmod(DistanceSinceMovementSound, StepDistance);
		PlayRandomCharacterSound(Sounds);
	}
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

void APlayerCharacter::UpdateAimWidget()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	const bool bCanShow = [&]()
	{
		if (!AimWidgetClass) return false;
		if (!IsValid(PlayerController)) return false;
		if (!PlayerController->IsLocalPlayerController()) return false;
		if (!CanThrowKunai()) return false;

		// 벽에서는 조준만 허용한다. 실제 투척은 입력과 어빌리티 양쪽에서 차단한다.
		bool bCanAim = GetCharacterMovement()->IsMovingOnGround();
		if (IsWallAttached()) bCanAim = true;
		if (!bCanAim) return false;

		return PlayerController->GetMousePosition(MouseX, MouseY);
	}();

	SetAimWidgetVisible(bCanShow);
	if (bCanShow && AimWidget)
	{
		// 조준점은 마우스 위치를 그대로 따라간다.
		AimWidget->SetPositionInViewport(FVector2D(MouseX, MouseY), true);
	}
}

void APlayerCharacter::SetAimWidgetVisible(bool bVisible)
{
	if (bAimWidgetVisible == bVisible)
	{
		return;
	}
	bAimWidgetVisible = bVisible;

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (bVisible)
	{
		if (!AimWidget && PlayerController && AimWidgetClass)
		{
			AimWidget = CreateWidget<UUserWidget>(PlayerController, AimWidgetClass);
			if (AimWidget)
			{
				AimWidget->AddToViewport(10);
				// 위젯 중앙이 조준 지점에 오도록 한다.
				AimWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
			}
		}
		if (AimWidget)
		{
			// 클릭이 위젯에 막히지 않도록 히트 테스트를 끈다.
			AimWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		if (PlayerController && bHideMouseCursorWhileAiming)
		{
			SavedMouseCursor = PlayerController->CurrentMouseCursor;
			// bShowMouseCursor를 끄면 마우스가 캡처되어 커서 위치가 고정되므로, 모양만 None으로 바꿔 숨긴다.
			PlayerController->CurrentMouseCursor = EMouseCursor::None;
		}
	}
	else
	{
		if (AimWidget)
		{
			AimWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (PlayerController && bHideMouseCursorWhileAiming
			&& PlayerController->CurrentMouseCursor == EMouseCursor::None)
		{
			PlayerController->CurrentMouseCursor = SavedMouseCursor;
		}
	}
}

void APlayerCharacter::UpdateCursorFacing(float DeltaSeconds)
{

	if (IsValid(ASC))
	{
		// 매달려 있거나 모서리에서 오르내리는 중에는 벽을 바라본 채로 둔다.
		if (ASC->HasMatchingGameplayTag(UGAWallAttach::GetWallStateTag())
			|| ASC->HasMatchingGameplayTag(UGAWallSlide::GetWallSlideAbilityTag()))
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

const UInputAction* APlayerCharacter::GetAbilityInputAction(EAbilityInputID InputID) const
{
    switch (InputID)
    {
    case EAbilityInputID::BasicAttack:
    case EAbilityInputID::ThrowKunai:
        return BasicAttackAction;
    case EAbilityInputID::CloneTech:
        return CloneTechAction;
    case EAbilityInputID::ElementalFire:
        return ElementalFireAction;
    case EAbilityInputID::SwiftStrike:
        return SwiftStrikeAction;
    case EAbilityInputID::Parry:
        return ParryAction;
    default:
        return nullptr;
    }
}

void APlayerCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (ParryAction)
		{
			EnhancedInputComponent->BindAction(ParryAction, ETriggerEvent::Started, this, &APlayerCharacter::DoParry);
		}
		if (CloneTechAction)
		{
			EnhancedInputComponent->BindAction(CloneTechAction, ETriggerEvent::Started, this, &APlayerCharacter::DoCloneTech);
			EnhancedInputComponent->BindAction(CloneTechAction, ETriggerEvent::Completed, this, &APlayerCharacter::DoCloneTechEnd);
			//EnhancedInputComponent->BindAction(CloneTechAction, ETriggerEvent::Canceled, this, &APlayerCharacter::DoCloneTechEnd);
		}
		if (CloneRecallAction)
		{
			EnhancedInputComponent->BindAction(CloneRecallAction, ETriggerEvent::Started, this, &APlayerCharacter::DoCloneRecall);
		}
		if (MoveScrollAction)
		{
			EnhancedInputComponent->BindAction(MoveScrollAction, ETriggerEvent::Triggered, this, &APlayerCharacter::DoMouseScroll);
		}
    if (ElementalFireAction)
    {
        EnhancedInputComponent->BindAction(ElementalFireAction, ETriggerEvent::Started, this, &APlayerCharacter::DoElementalFire);
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

		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &APlayerCharacter::OnCameraMoveEndInput);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Canceled, this, &APlayerCharacter::OnCameraMoveEndInput);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &APlayerCharacter::DoCameraMoveStart);

		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &APlayerCharacter::DoCrouch);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &APlayerCharacter::DoUnCrouch);

		EnhancedInputComponent->BindAction(SwiftStrikeAction, ETriggerEvent::Started, this, &APlayerCharacter::DoSwiftStrike);
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
	PlayActionSound(EPlayerActionSound::Land, 0.75f);
	bCanCoyoteJump = false;

	// 공중에서 앉기 키를 누르고 있었다면 착지하면서 앉는다.
	if (bCrouchInputHeld && !IsWallAttached())
	{
		Crouch();
	}
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
	// 컨트롤러가 떨어지기 전에 조준 위젯을 숨기고 커서 모양을 되돌린다.
	SetAimWidgetVisible(false);
	ASC->CancelAllAbilities();
	Super::UnPossessed();
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
	if (IsParrying()) return;
	if (bIsDead) return;

	FVector2D MoveVector = Value.Get<FVector2D>();
	if (IsWallAttached())
	{
		DoWAllMove(MoveVector.X);
	}
	else
	{
		// Require a deliberate downward tilt (within about 39 degrees of down).
		// Small downward drift while moving sideways must not trigger ledge hanging.
		const float DownInput = -MoveVector.X;
		if (DownInput >= FMath::Clamp(LedgeHangDownInputThreshold, 0.1f, 1.0f)
			&& DownInput > FMath::Abs(MoveVector.Y) * 1.25f)
			TryActivateAbilityByInputID(EAbilityInputID::LedgeHang);

		DoMove(MoveVector.Y);
	}


}

void APlayerCharacter::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    if (GetGameInstance()->GetSubsystem<UGameSaveSubsystem>()->IsApplyingRestore()) return;
	// 체력이 0이면 사망하고, 살아 있는 상태의 체력 감소는 피격으로 처리한다.
	if (bIsDead)
	{
		return;
	}
	if (Data.NewValue < Data.OldValue && !IsValid(PendingAssassinationMontage))
	{
		PlayActionSound(EPlayerActionSound::Hit);
	}
	if (Data.NewValue <= 0.0f)
	{
		Die();
		return;
	}
	if (Data.NewValue < Data.OldValue)
	{
		FGameplayTagContainer ParryTags(EclipseGameplayTags::Ability_Parry);
		ASC->CancelAbilities(&ParryTags);
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

	// 숨은 자리에서 죽으면 이동 평면으로 되돌린 뒤 사망을 처리한다.
	if (IsValid(HidingComponent)) HidingComponent->ForceExit();

	//사망 처리
	bIsDead = true;
	DoCameraMoveEnd();
	DoCloneTechEnd();
	ASC->CancelAllAbilities();

	// 정지 후 움직이지 못하게 세팅
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// 사망 몽타주 재생. 암살당한 경우 암살 피격 몽타주로 대신한다.
	UAnimMontage* MontageToPlay = DeathMontage;
	if (IsValid(PendingAssassinationMontage))
	{
		MontageToPlay = PendingAssassinationMontage;
	}

	float MontageDuration = 0.0f;
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (MontageToPlay)
		{
			MontageDuration = AnimInstance->Montage_Play(MontageToPlay, 1.0f, EMontagePlayReturnType::Duration);
			if (MontageDuration > 0.0f)
			{
				// 쓰러진 자세를 유지한다.
				FOnMontageBlendingOutStarted BlendOutDelegate;
				BlendOutDelegate.BindWeakLambda(this, [this, MontageToPlay](UAnimMontage* Montage, bool bInterrupted)
				{
					if (bIsDead && Montage == MontageToPlay)
					{
						GetMesh()->bPauseAnims = true;
					}
				});
				AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, MontageToPlay);
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

void APlayerCharacter::ReceiveAssassination(UAnimMontage* Montage)
{
	if (bIsDead || !IsValid(ASC))
	{
		return;
	}

	// 기존 체력/사망 경로를 사용하고 이번 사망 연출만 암살 피격 몽타주로 바꾼다.
	PendingAssassinationMontage = Montage;
	ASC->SetNumericAttributeBase(UStatAttributeSet::GetHealthAttribute(), 0.0f);

	if (!bIsDead)
	{
		Die();
	}

	PendingAssassinationMontage = nullptr;
}

void APlayerCharacter::DoBasicAttack()
{
	if (IsParrying()) return;
	if (bIsDead) return;
	if (!ASC) return;
	if (IsAssassinating()) return;

	// 조준 중이 아니면 벽에 매달린 상태에서도 현재 대상에 맞는 암살을 시도한다.
	if (!bIsExploring && CanAssassinateCurrentTarget())
	{
		DoAssassinate();
		return;
	}

	if (!FindAbilitySpecByInputID(EAbilityInputID::BasicAttack)) return;
	if (!BasicAttackMontage) return;

	// 근접 공격은 땅에 서 있을 때만 나간다. 점프·낙하 중에는 공격하지 않는다.
	if (!GetCharacterMovement()->IsMovingOnGround()) return;

	// 공격 몽타주는 선 자세라서, 앉은 채 위가 막혀 있으면 벽을 뚫고 일어서 보이므로 공격하지 않는다.
	if (!HasRoomToStand()) return;

	if (bIsExploring)
	{
		DoThrowKunai();
		return;
	}
	// 첫 공격 전에만 방향을 확정하고, 콤보 추가 입력은 기존 공격 방향을 유지한다.
	if (!ASC->HasMatchingGameplayTag(UGABaseAttack::GetAttackingTag()))
	{
		SnapToFacingTarget();
	}

	Super::DoBasicAttack();
}

void APlayerCharacter::DoAssassinate()
{
	if (IsParrying()) return;
	if (bIsDead) return;
	if (!IsValid(ASC)) return;
	if (IsAssassinating()) return;
	if (!IsValid(AssassinateTargetingComponent)) return;

	UAssassinateTargetComponent* Target = AssassinateTargetingComponent->GetCurrentTarget();
	if (!IsValid(Target)) return;
	if (!Target->CanAssassinate(this)) return;

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

bool APlayerCharacter::IsHiding() const
{
	return IsValid(HidingComponent) && HidingComponent->IsHiding();
}

bool APlayerCharacter::IsWallAttached() const
{
	if (!IsValid(ASC)) return false;
	return ASC->HasMatchingGameplayTag(UGAWallAttach::GetWallAttachingTag());
}

bool APlayerCharacter::IsAttacking() const
{
	if (!IsValid(ASC)) return false;
	return ASC->HasMatchingGameplayTag(UGABaseAttack::GetAttackingTag());
}

bool APlayerCharacter::IsOnWall() const
{
	if (!IsValid(ASC)) return false;
	return ASC->HasMatchingGameplayTag(UGAWallAttach::GetWallStateTag())
		|| ASC->HasMatchingGameplayTag(UGAWallSlide::GetWallSlideAbilityTag());
}

bool APlayerCharacter::HasRoomToStand() const
{
	if (!bIsCrouched) return true;

	// 캡슐 바닥은 그대로 두고 서 있는 높이로 늘렸을 때 막히는지 본다. 바닥에 닿은 부분이 걸리지 않도록 아래를 조금 띄운다.
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float CrouchedHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float StandHalfHeight = GetDefaultHalfHeight();
	constexpr float FloorClearance = 2.0f;
	const FVector StandLocation = GetActorLocation() + FVector(0.0f, 0.0f, StandHalfHeight - CrouchedHalfHeight + FloorClearance);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(), FMath::Max(1.0f, StandHalfHeight - FloorClearance));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrouchStandCheck), false, this);
	FCollisionResponseParams ResponseParams;
	GetCharacterMovement()->InitCollisionParams(Params, ResponseParams);
	return !GetWorld()->OverlapBlockingTestByChannel(StandLocation, FQuat::Identity,
		Capsule->GetCollisionObjectType(), Shape, Params, ResponseParams);
}

void APlayerCharacter::SetAssassinationFacing(float Yaw)
{
	SetCursorFacingActive(false);
	FacingTargetYaw = Yaw;
	SnapToFacingTarget();
}

void APlayerCharacter::DoThrowKunai()
{
	if (IsOnWall()) return;
	if (!CanThrowKunai() || !ASC)
	{
		return;
	}

	// 커서 쪽으로 몸을 돌리는 것은 어빌리티가 실행된 뒤에 한다. 매달림 등으로 막히면 몸도 돌지 않는다.
	TryActivateAbilityByInputID(EAbilityInputID::ThrowKunai);
}

void APlayerCharacter::FaceCursorForThrow()
{
	if (IsParrying()) return;
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
}

void APlayerCharacter::DoMove(float Forward)
{
	if (IsParrying()) return;
	if (bIsDead || IsAssassinating()) return;

	// 공격이 끝나기 전에 반대 방향 입력으로 몸이 돌아가지 않게 한다.
	if (IsAttacking()) return;

	// 매달린 동안(모서리에서 오르내리는 중 포함) 일반 이동 입력이 목표 방향을 바꾸지 않게 한다.
	if (IsOnWall()) return;

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


void APlayerCharacter::DoJumpStart()
{
	if (IsAttacking()) return;
	if (IsParrying()) return;
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

	// 은신구역에 있는 동안 상호작용 입력은 나오기로만 쓴다. 들어가거나 나오는 중에는 무시된다.
	if (IsHiding())
	{
		HidingComponent->RequestExit();
		return;
	}

	//// 상호작용보다 암살을 우선하고, 실패하더라도 같은 입력으로 상호작용하지 않는다.
	//if (CanAssassinateCurrentTarget())
	//{
	//	DoAssassinate();
	//	return;
	//}

	this->InteractComponent->TryInteract();
}

bool APlayerCharacter::CanAssassinateCurrentTarget() const
{
	if (!IsValid(AssassinateTargetingComponent)) return false;
	UAssassinateTargetComponent* Target = AssassinateTargetingComponent->GetCurrentTarget();
	return IsValid(Target) && Target->CanAssassinate(this);
}

void APlayerCharacter::DoWAllMove(float Up)
{
	FGameplayAbilitySpec* Spec = FindAbilitySpecByInputID(EAbilityInputID::WallAttach);
	if (!Spec) return;
	const UGAWallAttach* WallAttach = Cast<UGAWallAttach>(Spec->GetPrimaryInstance());
	if (!WallAttach) return;

	const bool bClimbableWall = WallAttach->GetIsClimbable();

	// 벽타기 벽을 타고 내려가다가 아래가 끝났으면 매달림을 푼다.
	//  - 발 높이에 더 이상 벽이 없으면(벽·사다리 아래 끝) 그대로 떨어진다
	//  - 발 바로 아래가 바닥이면 내려선다
	// 풀지 않으면 벽 아래 끝을 지나서도, 바닥에 닿은 뒤에도 벽 타는 자세로 계속 내려가려 한다.
	if (bClimbableWall && IsWallAttached() && Up < -0.1f)
	{
		const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float WallDirX = GetActorForwardVector().X >= 0.f ? 1.f : -1.f;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(WallClimbBottom), false, this);

		FVector Feet = GetActorLocation();
		Feet.Z -= HalfHeight - 5.f;
		FHitResult Hit;
		const bool bWallAtFeet = GetWorld()->LineTraceSingleByChannel(Hit, Feet, Feet + FVector(WallDirX * 80.f, 0.f, 0.f), ECC_GameTraceChannel2, Params);
		const bool bGroundBelow = GetWorld()->LineTraceSingleByChannel(Hit, Feet, Feet - FVector(0.f, 0.f, 25.f), ECC_Visibility, Params);

		if (!bWallAtFeet || bGroundBelow)
		{
			FGameplayTagContainer WallAttachTags;
			WallAttachTags.AddTag(UGAWallAttach::GetWallAttachAbilityTag());
			GetAbilitySystemComponent()->CancelAbilities(&WallAttachTags);
			return;
		}
	}

	// 위 입력으로 모서리 위로 올라간다. 벽타기 벽에서는 모서리 매달림 높이에 온 뒤에만 올라간다.
	if (Up > 0.5f)
	{
		TryActivateAbilityByInputID(EAbilityInputID::LedgeClimb);
	}

	// 벽타기 벽에서는 위아래 입력으로 벽을 타고 움직인다. 올라가기가 시작됐으면 매달림이 끝나 있으므로 넣지 않는다.
	if (bClimbableWall && IsWallAttached())
	{
		// 벽타기 벽과 일반 벽이 이어져 있으면, 이동할 쪽 벽이 일반 벽일 때 그 자리에서 멈춘다.
		if (!FMath::IsNearlyZero(Up) && !WallAttach->CanClimbToward(this, Up))
		{
			GetCharacterMovement()->StopMovementImmediately();
			return;
		}

		//AddMovementInput(FVector::UpVector, Up);
		const float WallDir = GetActorForwardVector().X >= 0.f ? 1.f : -1.f;
		AddMovementInput(FVector(WallDir * 0.05f, 0.f, Up), 1.f);
	}
}

void APlayerCharacter::DoCameraMoveStart()
{
	// 공격 중(콤보, 표창, 질풍참 등)에는 조준 모드에 들어가지 않는다.
	if (IsAttacking()) return;

	APlayerCharacterController* APC = Cast<APlayerCharacterController>(GetController());
	if (!APC)
	{
		UE_LOG(LogTemp, Log, TEXT("[APlayerCharacter::DoCameraMoveStart] : APC Was nullptr"));
		return;
	}

	if (!bIsExploring)
	{
		APC->InitializeGamepadAimCursor();
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

void APlayerCharacter::DoCrouch()
{
	if (IsWallAttached())
	{
		// 매달린 상태에서 앉기를 누르면 벽 아래로 내려간다.
		TryActivateAbilityByInputID(EAbilityInputID::WallSlide);
	}
	else
	{
		// 키를 누르고 있는 동안 웅크린다.
		// 공중에서 앉으면 캡슐 중심 기준으로 줄어 카메라가 튀므로 착지할 때 앉는다.
		bCrouchInputHeld = true;
		if (GetCharacterMovement()->IsMovingOnGround())
		{
			Crouch();
		}
	}
}

void APlayerCharacter::DoUnCrouch()
{
	bCrouchInputHeld = false;
	UnCrouch();
}

void APlayerCharacter::DoSwiftStrike()
{
	if (IsParrying()) return;
	if (bIsDead) return;
	if (!ASC) return;
	if (!FindAbilitySpecByInputID(EAbilityInputID::SwiftStrike)) return;

	// 회전 중간 각도가 아니라 이동 입력·커서가 정한 최종 방향으로 돌진한다.
	SnapToFacingTarget();

	TryActivateAbilityByInputID(EAbilityInputID::SwiftStrike);
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
	// 타게팅 중에는 기본 IMC를 빼서 타게팅 IMC(확정, 취소)만 받는다.
	if (APlayerCharacterController* APC = Cast<APlayerCharacterController>(GetController()))
	{
		static const FName CloneTargetingLock(TEXT("CloneTargeting"));
		if (bTargeting)
		{
			APC->InitializeGamepadAimCursor(true);
			APC->PushInputLock(CloneTargetingLock);
		}
		else
		{
			APC->PopInputLock(CloneTargetingLock);
		}
	}
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
			bCanCoyoteJump = false;

			// 벽에서 튕겨 나가는 방향을 바라본다.
			FacingTargetYaw = JumpDirection.X > 0.0f ? 0.0f : 180.0f;
			SetActorRotation(FRotator(0.0f, FacingTargetYaw, 0.0f));

			FVector WallJumpImpulse = JumpDirection * WallJumpHorizontalImpulse;
			WallJumpImpulse.Z = GetCharacterMovement()->JumpZVelocity * WallJumpVerticalMultiplier;

			LaunchCharacter(WallJumpImpulse, true, true);
			PlayActionSound(EPlayerActionSound::Jump);
			// 벽 점프 시 소리 발생
			this->NoiseComponent->EmitNoise(ENoiseType::WallJump);
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

	// 앉은 상태에서는 점프가 막히므로 즉시 일어선다. 위가 막혀 있으면 앉은 채로 남아 점프하지 않는다.
	if (bIsCrouched)
	{
		UnCrouch();
		GetCharacterMovement()->UnCrouch(false);
	}

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

void APlayerCharacter::DoElementalFire()
{
	if (IsAssassinating()) return;
	if (bIsDead || !ASC) return;

	//ASC->AbilityLocalInputPressed(ToAbilityInputID(EAbilityInputID::ElementalFire));
	this->TryActivateAbilityByInputID(EAbilityInputID::ElementalFire);
}

void APlayerCharacter::DoCloneTechEnd()
{
	if (ASC) ASC->AbilityLocalInputReleased(ToAbilityInputID(EAbilityInputID::CloneTech));
}

void APlayerCharacter::DoCloneRecall()
{
	if (bIsDead) return;
	if (!IsValid(ASC)) return;
	if (this->PlayerCondition == EPlayerCondition::Combat)
	{
		UUIMessageSubsystem::ShowMessage(Cast<APlayerController>(
			GetController()), 
			FText::FromString(TEXT("적에게 발각시엔 사용할 수 없습니다."))
		);
		return;
	}
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
	if (bIsDead)
	{
		return;
	}

	// 세이프 포인트 볼륨을 아직 지나지 않았으면(이어하기·리스폰 직후) 현재 체크포인트로,
	// 그것도 없으면 레벨 시작 체크포인트(bActivateOnBeginPlay)로 옮긴다. 둘 다 없으면 옮기지 않는다.
	// 세이프 포인트는 캐릭터에만 기록되므로 새로 스폰된 캐릭터는 다음 볼륨을 밟기 전까지 갖고 있지 않다.
	FTransform TargetTransform = SafePointTransform;
	if (!bHasSafePoint)
	{
		const UCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UCheckpointSubsystem>();
		if (!Checkpoints || !Checkpoints->GetFallbackRespawnTransform(GetActorLocation(), TargetTransform))
		{
			return;
		}
	}

	// 평면 고정을 되돌린 뒤에 안전 지점 평면으로 옮긴다.
	if (IsValid(HidingComponent)) HidingComponent->ForceExit();

	// 매달리기·공격 등 진행 중인 어빌리티를 끝내고 옮긴다. 매달린 자세나 루트 모션 이동이 안전 지점까지 따라오지 않게 한다.
	// 분신술을 포함해 활성 어빌리티를 모두 취소한다.
	if (IsValid(ASC))
	{
		TArray<FGameplayAbilitySpecHandle> ActiveHandles;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (!Spec.IsActive()) continue;
			ActiveHandles.Add(Spec.Handle);
		}
		for (const FGameplayAbilitySpecHandle& Handle : ActiveHandles)
		{
			ASC->CancelAbilityHandle(Handle);
		}
	}
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetPlaneConstraintOrigin(TargetTransform.GetLocation());
	SetActorLocationAndRotation(
		TargetTransform.GetLocation(),
		TargetTransform.Rotator(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);
	FacingTargetYaw = GetActorForwardVector().X >= 0.f ? 0.f : 180.f;
}

void APlayerCharacter::PlayInteractMontage_Implementation(AActor* InInteractable, UAnimMontage* InMontage)
{
	// 방향 맞추기, 입력 잠금, 재생은 InteractComponent가 맡는다
	if (IsValid(InteractComponent)) InteractComponent->PlayInteractMontage(InInteractable, InMontage);
}

void APlayerCharacter::AddEngagedEnemy(AEnemy* InEnemy)
{
	if (!IsValid(InEnemy)) return;
	EngagedEnemies.Add(InEnemy);
	// 숨어 있다가 발각되면 은신이 풀린다. 나오는 연출은 OnPlayerConditionChanged(Combat)를 받은 쪽이 처리한다.
	SetPlayerCondition(EPlayerCondition::Combat);
}

void APlayerCharacter::RemoveEngagedEnemy(AEnemy* InEnemy)
{
	EngagedEnemies.Remove(InEnemy);
	// 알리지 못하고 사라진 적이 플레이어를 Combat에 묶어 두지 않도록 함께 정리한다.
	for (auto It = EngagedEnemies.CreateIterator(); It; ++It)
	{
		if (!It->IsValid()) It.RemoveCurrent();
	}

	// 전투가 끝나도 숨은 자리로 돌아가지 않는다. 발각될 때 이미 은신이 풀렸다.
	if (EngagedEnemies.Num() == 0 && PlayerCondition == EPlayerCondition::Combat)
		SetPlayerCondition(EPlayerCondition::Normal);
}

bool APlayerCharacter::TryEnterHiding()
{
	if (PlayerCondition == EPlayerCondition::Combat || bIsDead) return false;
	SetPlayerCondition(EPlayerCondition::Hidden);
	return true;
}

void APlayerCharacter::ExitHiding()
{
	if (PlayerCondition == EPlayerCondition::Hidden)
		SetPlayerCondition(EPlayerCondition::Normal);
}

void APlayerCharacter::SetPlayerCondition(EPlayerCondition InPlayerCondition)
{
	if (InPlayerCondition == PlayerCondition) return;
	PlayerCondition = InPlayerCondition;
	OnPlayerConditionChanged.Broadcast(PlayerCondition);
}

void APlayerCharacter::DoParry()
{
	if (bIsDead) return;
	if (!ASC) return;
	if (IsParrying()) return;

	// 패링 판정은 바라보는 방향 기준이므로, 회전 중간 각도가 아니라 이동 입력·커서가 정한 최종 방향으로 먼저 돌린다.
	SnapToFacingTarget();

	TryActivateAbilityByInputID(EAbilityInputID::Parry);
}

bool APlayerCharacter::IsParrying() const
{
    if (bParryMovementLocked) return true;
    if (!IsValid(ASC)) return false;

    return ASC->HasMatchingGameplayTag(EclipseGameplayTags::State_Parrying);
}

void APlayerCharacter::SetParryMovementLocked(bool bLocked)
{
    if (bParryMovementLocked == bLocked) return;

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (!Movement) return;

    if (bLocked)
    {
        bParryMovementLocked = true;

        // 걷기 모드를 유지해야 정지 동작 중에도 이동 발판을 따라간다.
        UAnimInstance* ParryAnimation = GetMesh()->GetAnimInstance();
        if (IsValid(ParryAnimation))
        {
            ParryLockedAnimation = ParryAnimation;
            SavedParryRootMotionMode = static_cast<uint8>(ParryAnimation->RootMotionMode);
            ParryAnimation->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
        }

        // 남아 있는 회전 동작과 커서 회전을 정리한 방향으로 고정한다.
        CancelMoveTurn();
        if (UPlayerCharacterAnimInstance* Animation = Cast<UPlayerCharacterAnimInstance>(GetMesh()->GetAnimInstance()))
        {
            Animation->StopTurnInPlace();
        }
        SetCursorFacingActive(false);

        StopJumping();
        ConsumeMovementInputVector();

        // 공중에서 이동을 끄면 그 자리에 떠 있게 되므로 수평 관성과 상승만 지우고 계속 떨어지게 한다.
        if (Movement->IsFalling())
        {
            Movement->Velocity.X = 0.0f;
            Movement->Velocity.Y = 0.0f;
            Movement->Velocity.Z = FMath::Min(Movement->Velocity.Z, 0.0f);
            return;
        }

        // 입력과 루트 이동만 잠그고 바닥 추적과 발판 이동은 계속 처리한다.
        Movement->StopMovementImmediately();
        return;
    }

    bParryMovementLocked = false;
    ConsumeMovementInputVector();

    if (UAnimInstance* Animation = ParryLockedAnimation.Get())
    {
        if (Animation->RootMotionMode == ERootMotionMode::IgnoreRootMotion)
        {
            Animation->SetRootMotionMode(static_cast<ERootMotionMode::Type>(SavedParryRootMotionMode));
        }
    }
    ParryLockedAnimation.Reset();
}

void APlayerCharacter::SetDebugInvincible(bool bEnable)
{
    bDebugInvincible = bEnable;
    SyncDebugInvincibleTag();
}

void APlayerCharacter::SyncDebugInvincibleTag()
{
    // 에디터 월드에 놓인 액터에서 체크를 바꿀 때는 값만 저장하고, 게임(PIE) 월드에서만 태그를 붙인다.
    if (!GetWorld() || !GetWorld()->IsGameWorld()) return;
    if (!IsValid(ASC) || bDebugInvincibleTagApplied == bDebugInvincible) return;

    if (bDebugInvincible)
    {
        ASC->AddLooseGameplayTag(EclipseGameplayTags::State_Invincible);
    }
    else
    {
        ASC->RemoveLooseGameplayTag(EclipseGameplayTags::State_Invincible);
    }
    bDebugInvincibleTagApplied = bDebugInvincible;
}

#if WITH_EDITOR
void APlayerCharacter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // PIE 중 디테일 패널에서 체크를 바꾸면 바로 태그를 맞춘다.
    if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(APlayerCharacter, bDebugInvincible))
    {
        SyncDebugInvincibleTag();
    }
}
#endif

void APlayerCharacter::ApplyUnlockedAbilities(const TArray<EAbilityInputID>& Unlocked)
{
    if (!HasAuthority() || !ASC) return;
    for (const FAbilityInfo& Info : UnlockableAbilities)
    {
        if (!Info.AbilityClass || !Unlocked.Contains(Info.InputID)) continue;
        if (!ASC->FindAbilitySpecFromClass(Info.AbilityClass))
        {
            ASC->GiveAbility(FGameplayAbilitySpec(Info.AbilityClass, FMath::Max(1, Info.AbilityLevel), ToAbilityInputID(Info.InputID), this));
            OnAbilityGranted.Broadcast(Info.InputID);
        }
    }
}
