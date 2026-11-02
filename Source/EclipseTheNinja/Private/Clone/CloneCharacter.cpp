// Fill out your copyright notice in the Description page of Project Settings.


#include "Clone/CloneCharacter.h"
#include "ObjectPool/ObjectPoolSubsystem.h"
#include "Component/InteractHighlightComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/WidgetComponent.h"
#include "Engine/TargetPoint.h"

// Sets default values
ACloneCharacter::ACloneCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	UCharacterMovementComponent* Movement = GetCharacterMovement();

	Movement->bRunPhysicsWithNoController = true;
	Movement->MaxWalkSpeed = 250.f;

	Movement->SetPlaneConstraintNormal(FVector::YAxisVector);
	Movement->bConstrainToPlane = true;
	// 컨트롤러 회전 대신 이동 방향으로 서서히 회전 (사이드뷰에서 정면을 거쳐 뒤를 돌아본다)
	this->bUseControllerRotationYaw = false;
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);

	this->InteractHighlight = CreateDefaultSubobject<UInteractHighlightComponent>(TEXT("InteractHighlight"));

	this->InteractKeyWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractKeyWidget"));
	this->InteractKeyWidget->SetupAttachment(GetRootComponent());
	this->InteractKeyWidget->SetWidgetSpace(EWidgetSpace::Screen);
	this->InteractKeyWidget->SetDrawAtDesiredSize(true);
	this->InteractKeyWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	this->InteractKeyWidget->SetVisibility(false);
}

// Called when the game starts or when spawned
void ACloneCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitializeWalkState();
	this->bIsSpawned = true;
	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetKeyWidget(this->InteractKeyWidget);
	}
	this->RefreshInteractState();
}

void ACloneCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 풀을 거치지 않고 파괴되는 경우
	NotifyDespawned();
	this->ReleaseTargetPlayer();

	Super::EndPlay(EndPlayReason);
}

void ACloneCharacter::InitializeWalkState()
{
	GetCharacterMovement()->SetPlaneConstraintOrigin(GetActorLocation());

	const float	Sign = GetActorForwardVector().X >= 0.f ? 1.f : -1.f;
	this->WalkDir = FVector(Sign, 0.f, 0.f);
}

void ACloneCharacter::NotifyDespawned()
{
	if (!this->bIsSpawned)
	{
		return ;
	}
	this->bIsSpawned = false;
	this->OnCloneDespawned.Broadcast(this);
}

void ACloneCharacter::LifeSpanExpired()
{
	UObjectPoolSubsystem::Release(this);
}

void ACloneCharacter::StopCloneMovement()
{
	this->bIsArrived = true;
	ConsumeMovementInputVector();
}

void ACloneCharacter::OnInteractPlayerMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (bInterrupted)
	{
		// 끊긴 경우에는 날리지 않고 입력만 돌려준다
		this->ReleaseTargetPlayer();
		return ;
	}

	this->LaunchTargetPlayer();
	//this->ResetInteractionCooldown();
}

void ACloneCharacter::ResetMoveProgress()
{
	this->TurnPauseRemaining = 0.f;
	this->WalkDirFlipCount = 0;
	this->OscillationElapsed = 0.f;
	this->bIsTargetUnreachable = false;
}

void ACloneCharacter::OnSpawnFromPool_Implementation()
{
	InitializeWalkState();
	this->bIsSpawned = true;
}

void ACloneCharacter::OnReturnToPool_Implementation()
{
	// LifeTime 타이머 초기화
	SetLifeSpan(0.f);
	NotifyDespawned();

	// 명령, 쿨다운, 안전 지점은 반환 시 정리한다
	// (안전 지점은 스폰 시 콜리전이 켜지며 오버랩으로 다시 등록될 수 있으므로 OnSpawnFromPool에서 지우지 않는다)
	this->bIsArrived = false;
	this->bIsCommandLocked = false;
	this->TargetActor = nullptr;
	this->TargetLocation = FVector::ZeroVector;
	this->CommandType = ECloneCommandType::Move;
	this->ResetMoveProgress();
	this->StopTargetMontage(this->InteractMontage);
	this->StopTargetMontage(this->InteractPlayerMontage);
	// 끊긴 몽타주의 콜백은 나중에 오거나 오지 않을 수 있으므로 여기서 바로 입력을 돌려준다
	this->ReleaseTargetPlayer();

	GetWorldTimerManager().ClearTimer(this->InteractionCooldownTimer);
	this->bIsInteractCooldown = false;
	this->SelectCount = 0;
	this->RefreshInteractState();

	this->SafePointLocation = FVector::ZeroVector;
	this->SafePointRotate = FRotator::ZeroRotator;

	ConsumeMovementInputVector();
	GetCharacterMovement()->StopMovementImmediately();
}

// Called every frame
void ACloneCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!this->bIsArrived)
	{
		FVector	LocationDiff = this->TargetLocation - GetActorLocation();
		if (LocationDiff.SquaredLength() > this->TargetLocationMargin)
		{
			if (LocationDiff.X * this->WalkDir.X < 0)
			{
				this->WalkDir *= -1.f;
				this->TurnPauseRemaining = this->TurnPauseTime;
				this->WalkDirFlipCount++;
			}
			// 처음 한 번은 목표 쪽으로 돌아서는 것일 수 있으므로, 두 번째 반전부터 왕복 시간을 잰다
			if (this->WalkDirFlipCount >= 2)
			{
				this->OscillationElapsed += DeltaTime;
				if (this->OscillationElapsed >= this->UnreachableOscillationTime)
				{
					UE_LOG(
						LogTemp,
						Log,
						TEXT("[ACloneCharacter::Tick] 목표 지점(%s)에 도달할 수 없어 이동을 멈춥니다."),
						*this->TargetLocation.ToString()
					);
					this->bIsTargetUnreachable = true;
					this->TurnPauseRemaining = 0.f;
					this->StopCloneMovement();
					return ;
				}
			}
			if (this->TurnPauseRemaining > 0.f)
			{
				const FRotator	TargetRot = this->WalkDir.Rotation();
			
				this->TurnPauseRemaining -= DeltaTime;
				SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), TargetRot, DeltaTime, 540.f));
			}
			else
			{
				AddMovementInput(this->WalkDir, 1.f, true);
			}
		}
		else
		{
			switch (this->CommandType)
			{
			case (ECloneCommandType::Move):
			{
				break;
			}
			case (ECloneCommandType::Interact):
			{
				IInteractable::Execute_Interact(this->TargetActor, this);
				this->PlayInteractMontage();
				break;
			}
			default:
				break;
			}
			ConsumeMovementInputVector();
			this->bIsArrived = true;
		}
	}
}

// Called to bind functionality to input
void ACloneCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void ACloneCharacter::SetCommand(const FCloneCommandType& InCommand)
{
	if (this->bIsCommandLocked)
	{
		return ;
	}
	this->ResetMoveProgress();
	this->StopTargetMontage(this->InteractMontage);
	this->StopTargetMontage(this->InteractPlayerMontage);
	this->CommandType = InCommand.Type;
	switch (InCommand.Type)
	{
	case (ECloneCommandType::Move):
	{
		this->TargetLocation = InCommand.TargetLocation;
		break;
	}
	case (ECloneCommandType::Interact):
	{
		this->TargetLocation = InCommand.TargetLocation;
		this->TargetActor = InCommand.TargetActor;
		break;
	}
	default:
		break;
	}
}

void ACloneCharacter::Interact_Implementation(AActor* InActor)
{
	if (!InActor || InActor != this->GetOwner())
	{
		return ;
	}
	ACharacter*	InCharacter = Cast<ACharacter>(InActor);
	if (!InCharacter)
	{
		return ;
	}
	if (bIsInteractCooldown)
	{
		return;
	}
	this->bIsInteractCooldown = true;
	//this->bIsInteracted = true;

	// 플레이어와 상호작용한 분신은 하던 명령을 버리고 그 자리에 멈추며, 이후 명령을 받지 않는다
	this->bIsCommandLocked = true;
	this->StopCloneMovement();
	GetCharacterMovement()->StopMovementImmediately();
	this->TargetActor = nullptr;
	this->CommandType = ECloneCommandType::Move;
	this->ResetMoveProgress();
	this->StopTargetMontage(this->InteractMontage);

	this->FaceToward(InCharacter);
	// 날아갈 때까지 플레이어는 제자리에서 입력을 받지 않는다
	this->TargetPlayer = InCharacter;
	if (APlayerController* PlayerController = Cast<APlayerController>(InCharacter->GetController()))
	{
		InCharacter->DisableInput(PlayerController);
	}
	InCharacter->GetCharacterMovement()->StopMovementImmediately();
	this->PlayInteractPlayerMontage();
	//this->RefreshInteractState();
	//InCharacter->LaunchCharacter(this->InteractVelocity, false, true);
}

void ACloneCharacter::FaceToward(const AActor* InActor)
{
	// 사이드뷰이므로 X축 방향만 맞춘다
	const float	ToActorX = InActor->GetActorLocation().X - GetActorLocation().X;
	if (FMath::IsNearlyZero(ToActorX) || ToActorX * GetActorForwardVector().X > 0.f)
	{
		return ;
	}
	this->WalkDir = FVector(FMath::Sign(ToActorX), 0.f, 0.f);
	SetActorRotation(this->WalkDir.Rotation());
}

void ACloneCharacter::ShowInteractionIndicator_Implementation()
{
	this->SelectCount++;
	this->RefreshHighlight();
}

void ACloneCharacter::HideInteractionIndicator_Implementation()
{
	this->SelectCount = FMath::Max(0, this->SelectCount - 1);
	this->RefreshHighlight();
}

bool ACloneCharacter::bIsInteractReady_Implementation()
{
	return (this->InteractState == EInteractState::Ready);
}

EInteractState ACloneCharacter::GetInteractState_Implementation()
{
	return (this->InteractState);
}

void ACloneCharacter::RefreshInteractState()
{
	// 분신은 몇 번이든 상호작용할 수 있으므로 Done이 되지 않는다
	this->InteractState = this->bIsInteractCooldown ? EInteractState::Cooldown : EInteractState::Ready;
	this->RefreshHighlight();
}

void ACloneCharacter::RefreshHighlight()
{
	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetHighlight(this->InteractState, (this->SelectCount > 0));
	}
}

void ACloneCharacter::RegisterSafePoint_Implementation(const ATargetPoint* InSafePoint)
{
	this->SafePointLocation = InSafePoint->GetActorLocation();
	this->SafePointRotate = InSafePoint->GetActorRotation();
}

void ACloneCharacter::ReturnToSafePoint_Implementation()
{
	if (this->SafePointLocation.IsNearlyZero())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[ACloneCharacter::ReturnToSafePoint] SafePoint가 등록되지 않았습니다.")
		);
		return;
	}
	// 데미지 입는 애니메이션
	// 데미지 입는 애니메이션 끝나면 부활하는 애니메이션
	this->SetActorLocation(this->SafePointLocation);
	this->SetActorRotation(this->SafePointRotate);
}

void ACloneCharacter::PlayInteractMontage()
{
	if (!this->InteractMontage)
	{
		return ;
	}
	UAnimInstance*	AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance)
	{
		AnimInstance->Montage_Play(this->InteractMontage);
	}
}

void ACloneCharacter::PlayInteractPlayerMontage()
{
	UAnimInstance*	AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (this->InteractPlayerMontage && AnimInstance
		&& AnimInstance->Montage_Play(this->InteractPlayerMontage) > 0.f)
	{
		// 블렌드 아웃이 시작될 때 플레이어를 날린다
		FOnMontageBlendingOutStarted	Delegate;
		Delegate.BindUObject(this, &ACloneCharacter::OnInteractPlayerMontageBlendingOut);
		AnimInstance->Montage_SetBlendingOutDelegate(Delegate, this->InteractPlayerMontage);
		return ;
	}
	// 몽타주가 없거나 재생에 실패하면 바로 날린다
	this->LaunchTargetPlayer();
}

void ACloneCharacter::LaunchTargetPlayer()
{
	if (this->TargetPlayer.IsValid())
	{
		this->TargetPlayer.Get()->LaunchCharacter(this->InteractVelocity, false, true);
	}
	this->ReleaseTargetPlayer();
	GetWorldTimerManager().SetTimer(
		InteractionCooldownTimer,
		this,
		&ACloneCharacter::ResetInteractionCooldown,
		this->InteractInterval,
		false
	);
}

void ACloneCharacter::ReleaseTargetPlayer()
{
	ACharacter*	Player = this->TargetPlayer.Get();
	this->TargetPlayer.Reset();
	if (!Player)
	{
		return ;
	}
	if (APlayerController* PlayerController = Cast<APlayerController>(Player->GetController()))
	{
		Player->EnableInput(PlayerController);
	}
}

void ACloneCharacter::StopTargetMontage(UAnimMontage* InMontage)
{
	if (!InMontage)
	{
		return ;
	}
	UAnimInstance*	AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && AnimInstance->Montage_IsPlaying(InMontage))
	{
		AnimInstance->Montage_Stop(this->InteractMontageBlendOutTime, InMontage);
	}
}

void	ACloneCharacter::ResetInteractionCooldown()
{
	this->bIsInteractCooldown = false;
	this->RefreshInteractState();
}
