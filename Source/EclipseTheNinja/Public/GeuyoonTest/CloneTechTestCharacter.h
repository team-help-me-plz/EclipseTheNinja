// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interface/Interactor.h"

#include "GameplayAbilitySpecHandle.h"
#include "GameplayEffectTypes.h"
#include "AbilitySystemInterface.h"
#include "Variant_SideScrolling/SideScrollingCharacter.h"
#include "CloneTechTestCharacter.generated.h"

class UAbilitySystemComponent;
class UInputAction;
class UBoxComponent;

USTRUCT(BlueprintType)
struct FAbilityInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 AbilityLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 InputID = INDEX_NONE;
};

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API ACloneTechTestCharacter : public ASideScrollingCharacter, public IAbilitySystemInterface, public IInteractor
{
	GENERATED_BODY()
	
public:
	ACloneTechTestCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void	RegisterSafePoint_Implementation(const FVector& InSafePoint) override;
	virtual void	ReturnToSafePoint_Implementation() override;

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void	GiveInitAbility();

	void	OnLocalInputStart(int32 InID);
	void	OnLocalInputCompleted(int32 InID);

	void			DoCloneTech();
	virtual void	DoInteract() override;

	UFUNCTION()
	void OnInteractBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBoxComponent>	InteractBox = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collision|Interact")
	FVector						InteractBoxSize = FVector(80.f, 1000.f, 100.f);

	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sphere|Radius")
	//float							InteractRadius = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	TArray<FAbilityInfo>		AbilityClassArray;

	//UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	//TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction>	CloneTechAction;

	TObjectPtr<AActor>			CloneTarget = nullptr;

private:
	TArray<AActor*>	OverlappedInteractActor;
	int32			OverlappedInteractActorTargetIndex = 0;
	TMap<int32, FGameplayAbilitySpecHandle>	AbilityHandles;

	FVector	SafePointLocation = FVector::ZeroVector;

	//static constexpr int32 InteractInputID = 100;
	static constexpr int32 CloneTechInputID = 100;

};


