

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GAS/Ability/AbilityAssetPreloader.h"
#include "GAWallSlide.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UGAWallSlide : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGAWallSlide();

	static FGameplayTag GetWallSlideAbilityTag();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;
	// 어빌리티를 받을 때 슬라이드 몽타주를 미리 불러 둔다. 처음 슬라이드할 때 로딩으로 멈추지 않게 한다.
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

protected:
	UFUNCTION()
	void OnSlideCompleted();
	UFUNCTION()
	void OnSlideInterrupted();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WallSlide", meta = (ClampMin = "0", Unit="cm"))
	float SlideMaxHeight = 1000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WallSlide", meta = (ClampMin = "1", Unit = "cm/s"))
	float SlideSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WallSlide|Anim")
	TSoftObjectPtr<UAnimMontage> SlideMontage;

	UPROPERTY(EditDefaultsOnly, Category = "WallSlide|Audio")
	TObjectPtr<USoundBase> SlideLoopSound;

private:
	void CheckSlideWall();
	FTimerHandle WallCheckTimer;
	bool bSlideMovementStarted = false;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> SlideAudioComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> LodaedSlideMontage;

	bool bSildeCompleted = false;

	// 미리 불러 둔 몽타주를 메모리에 잡아 둔다. 어빌리티를 잃으면 놓는다.
	FAbilityAssetPreloader AssetPreloader;
};
