// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interface/Interactable.h"
#include "Interface/CheckpointResettable.h"

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

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interact|Target")
	TObjectPtr<AActor>			TargetActor = nullptr;

	// 여러번 Interact 할 수 있는 액터인가
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interact|State")
	bool	bIsReinteractable = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact|Cooldown", meta = (ClampMin = "0.05"))
	float	InteractInterval = 0.5f;

	// 현재 플래그로 InteractState를 다시 계산하고 하이라이트를 갱신
	void	RefreshInteractState();
	void	RefreshHighlight();

	FTimerHandle	InteractionCooldownTimer;
	bool			bIsInteractCooldown = false;
	// Interact 함수가 한 번이라도 불렸는가
	bool			bIsInteracted = false;

private:

	EInteractState	InteractState = EInteractState::Ready;
	// 선택(강조)을 요청한 수. InteractBox 선택, 발판 강조, 분신 조준 커서가 각각 Show/Hide를 짝지어 호출한다
	int32			SelectCount = 0;
	// TargetActor의 Activate 테두리를 켜 둔 상태인가 (Show/Hide를 짝지어 호출하기 위함)
	bool			bIsShowingTargetIndicator = false;

};
