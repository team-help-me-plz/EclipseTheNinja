#include "Noise/AnimNotify_Noise.h"
#include "Component/NoiseComponent.h"

#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_Noise::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	if (!Character || MeshComp != Character->GetMesh())
	{
		return;
	}
	if (UNoiseComponent* NoiseComp = Character->FindComponentByClass<UNoiseComponent>())
	{
		NoiseComp->EmitNoise(this->NoiseType);
	}
}

FString UAnimNotify_Noise::GetNotifyName_Implementation() const
{
    // 타임라인에서 어떤 소음이 붙었는지 바로 보이게 한다.
	return (FString::Printf(TEXT("Noise: %s"), *UEnum::GetDisplayValueAsText(this->NoiseType).ToString()));
}
