// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interface/Activatable.h"
#include "Interface/Interactable.h"
#include "Interface/CheckpointResettable.h"
#include "Interface/VineLockable.h"

#include "GameFramework/Actor.h"
#include "BaseDoor.generated.h"

class UStaticMeshComponent;
class UCheckpointBoundComponent;
class UInteractHighlightComponent;
class UActivateHighlightComponent;
class UWidgetComponent;

UCLASS()
class ECLIPSETHENINJA_API ABaseDoor : public AActor, public IActivatable, public IInteractable, public ICheckpointResettable, public IVineLockable
{
	GENERATED_BODY()

public:
	ABaseDoor();

	virtual void	Tick(float DeltaTime) override;

	virtual void	ToggleActivate_Implementation() override;
	virtual void	ShowActivationIndicator_Implementation() override;
	virtual void	HideActivationIndicator_Implementation() override;

	virtual void	Interact_Implementation(AActor* InActor) override;
	virtual void	ShowInteractionIndicator_Implementation() override;
	virtual void	HideInteractionIndicator_Implementation() override;
	virtual bool	bIsInteractReady_Implementation() override;
	virtual EInteractState	GetInteractState_Implementation() override;

	// 체크포인트 리스폰: 열림 상태와 쿨다운을 BeginPlay 시점으로 되돌린다
	virtual void	ResetToInitial_Implementation() override;
	// 체크포인트 리스폰: 문 메시를 즉시 현재 상태 위치로 맞춘다 (열리고 닫히는 연출 없이)
	virtual void	OnAfterReset_Implementation() override;

	virtual void	DoorMovement(float DeltaTime);

	virtual void	AddInteractLock_Implementation() override;
	virtual void	RemoveInteractLock_Implementation() override;
	virtual bool	IsInteractLocked_Implementation() override;

protected:
	virtual void	BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent>			RootScene = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMeshComponent>	RightDoorMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMeshComponent>	LeftDoorMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCheckpointBoundComponent>	CheckpointBound = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInteractHighlightComponent>	InteractHighlight = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UActivateHighlightComponent>	ActivateHighlight = nullptr;

	// 선택되어 E키로 상호작용할 수 있을 때만 보이는 키 위젯. 위치와 위젯 클래스는 블루프린트에서 지정
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent>	InteractKeyWidget = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.01"))
	float	DoorMoveTime = 3.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Interactable", meta = (ClampMin = "0.01"))
	bool	bIsInteractable = true;

	UPROPERTY(Transient)
	double	OpenAlpha = 0.0;

	UPROPERTY(Transient)
	bool	bIsOpen = false;

private:
	void	ResetInteractionCooldown();

	// 현재 플래그로 InteractState를 다시 계산하고 하이라이트를 갱신
	void	RefreshInteractState();
	void	RefreshHighlight();

	EInteractState	InteractState = EInteractState::Ready;
	// 선택(강조)을 요청한 수. InteractBox 선택, 발판 강조, 분신 조준 커서가 각각 Show/Hide를 짝지어 호출한다
	int32			SelectCount = 0;

	FTimerHandle	InteractionCooldownTimer;
	bool			bIsInteractCooldown = false;
	float			InteractInterval = 0.5f;

	// 체크포인트 리셋용 초기 상태
	bool			bInitialIsOpen = false;

	int32	LockedVineCount = 0;

};
