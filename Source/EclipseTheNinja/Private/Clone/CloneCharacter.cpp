// Fill out your copyright notice in the Description page of Project Settings.


#include "Clone/CloneCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

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
}

// Called when the game starts or when spawned
void ACloneCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->SetPlaneConstraintOrigin(GetActorLocation());

	const float	Sign = GetActorForwardVector().X >= 0.f ? 1.f : -1.f;
	this->WalkDir = FVector(Sign, 0.f, 0.f);
}

// Called every frame
void ACloneCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!this->bIsArrived)
	{
		FVector	LocationDiff = GetActorLocation() - this->TargetLocation;
		if (LocationDiff.SquaredLength() > this->TargetLocationMargin)
		{
			AddMovementInput(this->WalkDir, 1.f, true);
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
	InCharacter->LaunchCharacter(this->InteractVelocity, false, true);
}

void ACloneCharacter::ShowInteractionIndicator_Implementation()
{
}

void ACloneCharacter::HideInteractionIndicator_Implementation()
{
}

bool ACloneCharacter::bIsInteractReady_Implementation()
{
	return (true);
}

void ACloneCharacter::RegisterSafePoint_Implementation(const FVector& InSafePoint)
{
	this->SafePointLocation = InSafePoint;
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
}

void	ACloneCharacter::ResetInteractionCooldown()
{
	this->bIsInteractCooldown = false;
}
