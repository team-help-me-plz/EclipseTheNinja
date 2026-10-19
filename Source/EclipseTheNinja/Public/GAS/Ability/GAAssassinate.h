

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAAssassinate.generated.h"

class APlayerCharacter;
class AEnemy;
class UAssassinateTargetComponent;
class UBrainComponent;
class UAnimMontage;

/**
 *
 */
UCLASS()
class ECLIPSETHENINJA_API UGAAssassinate : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGAAssassinate();
	static FGameplayTag GetAssassinatingTag();

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
	UPROPERTY(EditDefaultsOnly, Category = "Assassination")
	TSoftObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Assassination")
	TSoftObjectPtr<UAnimMontage> VictimMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Assassination", meta = (ClampMin = "0.05", Units = "s"))
	float ApproachTimeout = 1.0f;

	/** 암살 접근 후 적 뒤에 유지할 중심 간 거리. 캡슐이 겹치지 않는 최소 거리로 보정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassination", meta = (ClampMin = "0.0", Units = "cm"))
	float BehindDistance = 150.0f;

private:
	UFUNCTION()
	void OnApproachCompleted();
	UFUNCTION()
	void OnCompleted();
	UFUNCTION()
	void OnInterrupted();
	UFUNCTION()
	void OnVictimDestroyed(AActor* DestroyedActor);

	TWeakObjectPtr<APlayerCharacter> Assassin;
	TWeakObjectPtr<AEnemy> Victim;
	TWeakObjectPtr<UAssassinateTargetComponent> TargetComponent;
	TWeakObjectPtr<UBrainComponent> PausedBrain;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ExecutionMontage;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> VictimExecutionMontage;

	FVector ApproachLocation = FVector::ZeroVector;
	FRotator ExecutionRotation = FRotator::ZeroRotator;
	TEnumAsByte<EMovementMode> PlayerMovementMode = MOVE_Walking;
	TEnumAsByte<EMovementMode> VictimMovementMode = MOVE_Walking;
	uint8 PlayerCustomMode = 0;
	uint8 VictimCustomMode = 0;
	bool bMovementLocked = false;
	bool bTargetReserved = false;
	bool bEnding = false;
};
