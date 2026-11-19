#include "Component/FootstepSoundComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

UFootstepSoundComponent::UFootstepSoundComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFootstepSoundComponent::SetFootstepSounds(const TArray<USoundBase*>& InSounds)
{
	this->FootstepSounds.Reset();

	for (USoundBase* Sound : InSounds)
	{
		if (Sound)
		{
			this->FootstepSounds.Add(Sound);
		}
	}

	this->DistanceSinceStep = 0.0f;
}

void UFootstepSoundComponent::SetAttenuation(USoundAttenuation* InAttenuation)
{
	this->Attenuation = InAttenuation;
}

void UFootstepSoundComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction
)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (this->FootstepSounds.IsEmpty() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 멈추거나 공중에 뜨면 다시 처음부터 센다 (플레이어 발소리와 같은 규칙)
	const float	Speed = this->GetOwnerWalkSpeed();

	if (Speed < this->MinMoveSpeed)
	{
		this->DistanceSinceStep = 0.0f;
		return;
	}

	// 달릴 때는 보폭이 길어지므로 더 긴 간격으로 발소리를 낸다
	const bool	bRunning = this->RunSpeedThreshold > 0.0f && Speed >= this->RunSpeedThreshold;
	const float	NearStepDistance = bRunning
		? this->RunStepDistance
		: this->StepDistance;

	// 플레이어 주변 범위 밖에서는 더 긴 간격으로 발소리를 낸다
	const float	CurrentStepDistance = this->IsOwnerNearPlayer()
		? NearStepDistance
		: FMath::Max(NearStepDistance, this->OffscreenStepDistance);

	this->DistanceSinceStep += Speed * DeltaTime;
	if (this->DistanceSinceStep >= CurrentStepDistance)
	{
		this->DistanceSinceStep = FMath::Fmod(this->DistanceSinceStep, CurrentStepDistance);
		this->PlayRandomFootstep();
	}
}

float UFootstepSoundComponent::GetOwnerWalkSpeed() const
{
	const ACharacter*	Character = Cast<ACharacter>(GetOwner());
	if (!Character || Character->IsHidden())
	{
		return 0.0f;
	}

	const UCharacterMovementComponent*	Movement = Character->GetCharacterMovement();
	if (!Movement || !Movement->IsMovingOnGround())
	{
		return 0.0f;
	}

	return Character->GetVelocity().Size2D();
}

bool UFootstepSoundComponent::IsOwnerNearPlayer() const
{
	const APawn*	Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return true;
	}

	// 사이드뷰라 화면 가로는 월드 X, 세로는 월드 Z다
	const FVector	Delta = GetOwner()->GetActorLocation() - Player->GetActorLocation();

	return FMath::Abs(Delta.X) <= this->OnScreenRange.X
		&& FMath::Abs(Delta.Z) <= this->OnScreenRange.Y;
}

void UFootstepSoundComponent::PlayRandomFootstep() const
{
	USoundBase*	Sound = this->FootstepSounds[FMath::RandRange(0, this->FootstepSounds.Num() - 1)];
	if (!Sound)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		this,
		Sound,
		GetOwner()->GetActorLocation(),
		this->Volume,
		1.0f,
		0.0f,
		this->Attenuation
	);
}
