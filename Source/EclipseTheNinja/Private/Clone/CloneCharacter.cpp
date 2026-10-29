// Fill out your copyright notice in the Description page of Project Settings.


#include "Clone/CloneCharacter.h"
#include "ObjectPool/ObjectPoolSubsystem.h"
#include "Component/InteractHighlightComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
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
	this->TargetActor = nullptr;
	this->TargetLocation = FVector::ZeroVector;
	this->CommandType = ECloneCommandType::Move;

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
	GetWorldTimerManager().SetTimer(
		InteractionCooldownTimer,
		this,
		&ACloneCharacter::ResetInteractionCooldown,
		this->InteractInterval,
		false
	);
	this->RefreshInteractState();
	InCharacter->LaunchCharacter(this->InteractVelocity, false, true);
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

void	ACloneCharacter::ResetInteractionCooldown()
{
	this->bIsInteractCooldown = false;
	this->RefreshInteractState();
}
