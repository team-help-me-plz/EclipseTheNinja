

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GAWallSlide.generated.h"

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

private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> LodaedSlideMontage;

	bool bSildeCompleted = false;
};
