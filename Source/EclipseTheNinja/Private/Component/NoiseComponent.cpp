


#include "Component/NoiseComponent.h"
#include "Noise/NoiseTypes.h"
#include "Subsystem/NoiseSubsystem.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values for this component's properties
UNoiseComponent::UNoiseComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UNoiseComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->LandedDelegate.AddDynamic(this, &UNoiseComponent::HandleLanded);
	}
}


// Called every frame
void UNoiseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UNoiseComponent::EmitNoise(ENoiseType InType)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	if (InType == ENoiseType::Wallstep && !Character->GetCharacterMovement()->IsMovingOnGround())
	{
		return ;
	}
	UNoiseSubsystem::MakeSomeNoise(this, InType, Character->GetNavAgentLocation(), Character);
}

void UNoiseComponent::HandleLanded(const FHitResult& Hit)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	// Landed 시점에는 아직 착지 전 낙하 속도가 남아 있다.
	if (!Character || -Character->GetVelocity().Z < MinLandSpeed)
	{
		return;
	}
	EmitNoise(ENoiseType::Land);
}

