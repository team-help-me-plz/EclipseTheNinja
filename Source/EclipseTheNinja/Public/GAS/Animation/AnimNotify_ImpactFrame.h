#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Subsystem/ImpactFrameSubsystem.h"
#include "AnimNotify_ImpactFrame.generated.h"

/**
 * 애니메이션의 임팩트 프레임 연출을 재생하는 노티파이.
 * 화면을 섬광 머티리얼(2톤 등)로 잠깐 덮고, 히트스톱과 카메라 흔들림을 함께 준다. 값은 노티파이마다 정한다.
 * 실제 재생은 UImpactFrameSubsystem이 맡으며, 애니메이션 에디터 미리보기에서는 아무것도 하지 않는다.
 */
UCLASS(meta = (DisplayName = "Impact Frame"))
class ECLIPSETHENINJA_API UAnimNotify_ImpactFrame : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_ImpactFrame();

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Impact Frame", meta = (ShowOnlyInnerProperties))
	FImpactFrameSettings Settings;
};
