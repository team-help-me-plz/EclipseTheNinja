// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interface/Interactable.h"
#include "Interface/CheckpointResettable.h"
#include "Save/GameSave.h"

#include "GameFramework/Actor.h"
#include "InteractSwitch.generated.h"

class AMeshAssembly;
class USphereComponent;
class UCheckpointBoundComponent;
class UInteractHighlightComponent;
class UWidgetComponent;

UCLASS()
class ECLIPSETHENINJA_API AInteractSwitch : public AActor, public IInteractable, public ICheckpointResettable
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AInteractSwitch();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void PostLoad() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void	Interact_Implementation(AActor* InActor) override;
	virtual void	ShowInteractionIndicator_Implementation() override;
	virtual void	HideInteractionIndicator_Implementation() override;

	// 체크포인트 리스폰: 한 번도 상호작용하지 않은 상태로 되돌린다
	virtual void	ResetToInitial_Implementation() override;
	virtual void	OnAfterReset_Implementation() override {}
	virtual bool	bIsInteractReady_Implementation() override;
	virtual EInteractState	GetInteractState_Implementation() override;

protected:
	virtual void	ResetInteractionCooldown();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent>			RootScene = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UChildActorComponent>	MeshAssembly = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCheckpointBoundComponent>	CheckpointBound = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInteractHighlightComponent>	InteractHighlight = nullptr;

	// 선택되어 E키로 상호작용할 수 있을 때만 보이는 키 위젯. 위치와 위젯 클래스는 블루프린트에서 지정
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent>	InteractKeyWidget = nullptr;

	// 작동시킬 대상 목록. IActivatable을 구현한 대상만 함께 토글된다
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interact|Target")
	TArray<TObjectPtr<AActor>>	TargetActors;

	// 구버전 단일 대상. 이미 저장된 레벨을 불러올 때 PostLoad에서 TargetActors로 옮기고 비운다
	UPROPERTY()
	TObjectPtr<AActor>			TargetActor = nullptr;

	// TargetActors 중 IActivatable을 구현한 대상이 하나라도 있는가
	bool	HasActivatableTarget() const;
	// 모든 대상의 Activate 상태를 토글한다
	void	ToggleTargets();
	// 모든 대상의 Activate 테두리를 켜거나 끈다
	void	SetTargetIndicator(bool bShow);

	// 여러번 Interact 할 수 있는 액터인가
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interact|State")
	bool	bIsReinteractable = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact|Cooldown", meta = (ClampMin = "0.05"))
	float	InteractInterval = 0.5f;

	// 현재 플래그로 InteractState를 다시 계산하고 하이라이트를 갱신
	void	RefreshInteractState();
	void	RefreshHighlight();

	// 스위치 상태를 세이브용 값으로 만든다. 레버는 On 자세를 더한다
	virtual FSavedActorState	MakeSaveState() const;
	// 이어하기로 받은 상태를 적용한다. 대상 액터는 자기 상태를 따로 복원하므로 다시 작동시키지 않는다
	virtual void	ApplySaveState(const FSavedActorState& State);
	// 대상을 작동시킨 직후 호출해 스위치 상태를 세이브에 기록한다
	void	SaveSwitchState() const;

	FTimerHandle	InteractionCooldownTimer;
	bool			bIsInteractCooldown = false;
	// Interact 함수가 한 번이라도 불렸는가
	bool			bIsInteracted = false;

private:

	EInteractState	InteractState = EInteractState::Ready;
	// 선택(강조)을 요청한 수. InteractBox 선택, 발판 강조, 분신 조준 커서가 각각 Show/Hide를 짝지어 호출한다
	int32			SelectCount = 0;
	// TargetActors의 Activate 테두리를 켜 둔 상태인가 (Show/Hide를 짝지어 호출하기 위함)
	bool			bIsShowingTargetIndicator = false;

};
