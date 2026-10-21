// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interface/Interactable.h"
#include "Interface/Activatable.h"
#include "Interface/CheckpointResettable.h"

#include "GameFramework/Actor.h"
#include "PressureSwitch.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UCheckpointBoundComponent;
class UInteractHighlightComponent;

UCLASS()
class ECLIPSETHENINJA_API APressureSwitch : public AActor, public IInteractable, public ICheckpointResettable
{
	GENERATED_BODY()

public:
	APressureSwitch();

	virtual void	Tick(float DeltaTime) override;

	virtual void	Interact_Implementation(AActor* InActor) override;
	virtual void	ShowInteractionIndicator_Implementation() override;
	virtual void	HideInteractionIndicator_Implementation() override;
	virtual bool	bIsInteractReady_Implementation() override;
	virtual EInteractState	GetInteractState_Implementation() override;

	// 체크포인트 리스폰: 한 번도 밟히지 않은 상태로 되돌린다
	virtual void	ResetToInitial_Implementation() override;
	virtual void	OnAfterReset_Implementation() override {}

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnPressureBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnPressureEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UBoxComponent>	PressureTrigger = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UChildActorComponent>	MeshAssembly = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCheckpointBoundComponent>	CheckpointBound = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInteractHighlightComponent>	InteractHighlight = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interact|Target")
	TObjectPtr<AActor>			TargetActor = nullptr;

	// PressureTrigger 밖으로 플레이어 혹은 분신이 나갔을 때 TargetActor의 Activate 상황을 다시 토글할 것인가
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interact|State")
	bool	bIsUnpressure = false;

	// 여러번 Interact 할 수 있는 액터인가
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interact|State")
	bool	bIsReinteractable = false;

private:
	// 현재 플래그로 InteractState를 다시 계산하고 하이라이트를 갱신
	void	RefreshInteractState();
	void	RefreshHighlight();

	// Interact 함수가 한 번이라도 불렸는가
	bool	bIsInteracted = false;

	EInteractState	InteractState = EInteractState::Ready;
	// 선택(강조)을 요청한 수. InteractBox 선택, 발판 강조, 분신 조준 커서가 각각 Show/Hide를 짝지어 호출한다
	int32			SelectCount = 0;
	// TargetActor의 Activate 테두리를 켜 둔 상태인가 (Show/Hide를 짝지어 호출하기 위함)
	bool			bIsShowingTargetIndicator = false;
};


