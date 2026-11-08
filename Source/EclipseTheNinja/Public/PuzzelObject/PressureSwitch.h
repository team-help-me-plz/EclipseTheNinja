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
class USoundBase;

UCLASS()
class ECLIPSETHENINJA_API APressureSwitch : public AActor, public IInteractable, public ICheckpointResettable
{
	GENERATED_BODY()

public:
	// 하위 클래스가 기본 컴포넌트 생성을 건너뛸 수 있도록 ObjectInitializer를 받는다
	APressureSwitch(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	virtual void PostLoad() override;
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

	// PressureTrigger 밖으로 플레이어 혹은 분신이 나갔을 때 TargetActors의 Activate 상황을 다시 토글할 것인가
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interact|State")
	bool	bIsUnpressure = false;

	// 여러번 Interact 할 수 있는 액터인가
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interact|State")
	bool	bIsReinteractable = false;

	// 발판이 눌려 대상을 작동시켰을 때 재생할 사운드. 비워 두면 재생하지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase>	PressSound = nullptr;

private:
	// 현재 플래그로 InteractState를 다시 계산하고 하이라이트를 갱신
	void	RefreshInteractState();
	void	RefreshHighlight();

	// Interact 함수가 한 번이라도 불렸는가
	bool	bIsInteracted = false;

	EInteractState	InteractState = EInteractState::Ready;
	// 선택(강조)을 요청한 수. InteractBox 선택, 발판 강조, 분신 조준 커서가 각각 Show/Hide를 짝지어 호출한다
	int32			SelectCount = 0;
	// 지금 발판을 누르고 있는 컴포넌트(플레이어·분신·몬스터 캡슐). 첫 번째가 올라올 때와 마지막이 내려갈 때만 작동한다
	TSet<TWeakObjectPtr<UPrimitiveComponent>>	PressingComponents;
	// 사라진(회수된 분신 등) 컴포넌트를 빼고 남은 수를 돌려준다
	int32			CountPressingComponents();
	// 지금 눌린(대상을 작동시킨) 상태인가. 모두 내려가면 풀린다
	bool			bPressedActive = false;
	// TargetActors의 Activate 테두리를 켜 둔 상태인가 (Show/Hide를 짝지어 호출하기 위함)
	bool			bIsShowingTargetIndicator = false;
};


