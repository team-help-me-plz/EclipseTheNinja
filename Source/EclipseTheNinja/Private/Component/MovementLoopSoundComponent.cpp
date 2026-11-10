#include "Component/MovementLoopSoundComponent.h"

#include "Components/AudioComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UMovementLoopSoundComponent::UMovementLoopSoundComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// 걷기 시작·멈춤 판정은 0.1초 간격이면 충분하다
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UMovementLoopSoundComponent::SetLoopSound(USoundBase* InSound)
{
	this->LoopSound = InSound;
	if (!this->LoopSound)
	{
		this->StopImmediately();
	}
}

void UMovementLoopSoundComponent::StopImmediately()
{
	if (IsValid(this->LoopAudio))
	{
		this->LoopAudio->Stop();
	}
	this->bIsPlaying = false;
}

void UMovementLoopSoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	this->StopImmediately();
	this->LoopAudio = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UMovementLoopSoundComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!this->LoopSound)
	{
		return;
	}

	const bool	bShouldPlay = this->IsOwnerWalking();

	if (bShouldPlay && !this->bIsPlaying)
	{
		this->StartLoop();
	}
	else if (!bShouldPlay && this->bIsPlaying)
	{
		this->StopLoop();
	}
}

bool UMovementLoopSoundComponent::IsOwnerWalking() const
{
	const ACharacter*	Character = Cast<ACharacter>(GetOwner());
	if (!Character || Character->IsHidden())
	{
		return false;
	}

	const UCharacterMovementComponent*	Movement = Character->GetCharacterMovement();
	if (!Movement || !Movement->IsMovingOnGround())
	{
		return false;
	}

	return Character->GetVelocity().Size2D() >= this->MinMoveSpeed;
}

void UMovementLoopSoundComponent::StartLoop()
{
	// 주인을 따라다니도록 붙여서 만들고, 멈출 때 파괴되지 않게 해 다시 쓴다
	if (!IsValid(this->LoopAudio))
	{
		this->LoopAudio = UGameplayStatics::SpawnSoundAttached(
			this->LoopSound,
			GetOwner()->GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset,
			false,
			this->Volume,
			1.0f,
			0.0f,
			nullptr,
			nullptr,
			false
		);
		if (!IsValid(this->LoopAudio))
		{
			return;
		}
		this->LoopAudio->Stop();
	}

	this->LoopAudio->SetSound(this->LoopSound);
	this->LoopAudio->SetVolumeMultiplier(this->Volume);

	// 긴 루프의 임의 지점부터 시작해 걷는 길이만큼만 들리게 한다.
	// 루프 사운드는 GetDuration()이 무한대를 돌려주므로 실제 길이(Duration)를 쓴다
	const float	Duration = this->LoopSound->Duration;
	const float	StartTime = (this->bRandomStartTime && Duration > 0.0f && Duration < INDEFINITELY_LOOPING_DURATION)
		? FMath::FRandRange(0.0f, Duration)
		: 0.0f;

	this->LoopAudio->FadeIn(this->FadeInTime, 1.0f, StartTime);
	this->bIsPlaying = true;
}

void UMovementLoopSoundComponent::StopLoop()
{
	if (IsValid(this->LoopAudio))
	{
		this->LoopAudio->FadeOut(this->FadeOutTime, 0.0f);
	}
	this->bIsPlaying = false;
}
