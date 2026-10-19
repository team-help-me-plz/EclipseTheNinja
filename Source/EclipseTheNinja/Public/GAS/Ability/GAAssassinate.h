

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
	// 암살 몽타쥬
	UPROPERTY(EditDefaultsOnly, Category = "Assassination")
	TSoftObjectPtr<UAnimMontage> AttackMontage;

	// 암상 당하는 사람의 몽타쥬
	UPROPERTY(EditDefaultsOnly, Category = "Assassination")
	TSoftObjectPtr<UAnimMontage> VictimMontage;

	// 접근 시간
	UPROPERTY(EditDefaultsOnly, Category = "Assassination", meta = (ClampMin = "0.05", Units = "s"))
	float ApproachTimeout = 1.0f;

private:
	// 암살을 하기위해서 접근이 끝났을 떄 호출될 함수
	UFUNCTION()
	void OnApproachCompleted();
	
	// 암살이 성공적으로 끝났을 때 호출될 함수
	UFUNCTION()
	void OnCompleted();

	// 암살이 도중에 종료된 경우 호출될 함수
	UFUNCTION()
	void OnInterrupted();

	// 암살대상이 파괴될때 호출될 함수
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
