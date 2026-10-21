// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Clone/CloneCommandType.h"

#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitCloneTargeting.generated.h"

class APlayerController;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class UNiagaraSystem;
class UNiagaraComponent;

// 마우스 좌클릭으로 분신에게 명령 전달
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCloneTargetAccepted, const FCloneCommandType&, Command);
// 마우스 우클릭으로 취소
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCloneTargetCancelled);
/**
 *
 */
UCLASS()
class ECLIPSETHENINJA_API UAbilityTask_WaitCloneTargeting : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAbilityTask_WaitCloneTargeting();

	virtual void	Activate() override;
	virtual void	TickTask(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks",
		meta = (HidePin = "OwningAbility", 	DefaultToSelf = "OwningAbility",BlueprintInternalUseOnly = "true"))
	static UAbilityTask_WaitCloneTargeting*	WaitCloneTarget(
		UGameplayAbility* OwningAbility,
		UInputMappingContext* MappingContext,
		UInputAction* AcceptAction,
		UInputAction* CancelAction,
		UNiagaraSystem* DestinataionNS,
		int32 MappingPriority = 100,
		float SlowMotionScale = 0.2f
	);

	UPROPERTY(BlueprintAssignable)
	FCloneTargetAccepted	OnAccepted;

	UPROPERTY(BlueprintAssignable)
	FCloneTargetCancelled	OnCancelled;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|NS")
	TObjectPtr<UNiagaraSystem>		DestinationSystem = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|NS")
	TObjectPtr<UNiagaraComponent>	DestinationEffect;

private:
	void HandleAccept();

	void HandleCancel();

	void UpdateTargetCandidate();

	void CleanupTargeting();

	// 커서 아래의 상호작용 액터 테두리를 강조. 이전 액터의 강조는 해제한다
	void SetHoveredActor(AActor* InActor);

	// GA에서 전달받은 설정
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext>	TargetingMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction>			AcceptInputAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction>			CancelInputAction;

	int32	TargetingMappingPriority = 100;
	float	TargetingTimeDilation = 0.2f;

	// 외부 객체는 소유하지 않고 참조
	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController>		PlayerController;

	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputComponent>	EnhancedInputComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem>	InputSubsystem;

	// 현재 선택 후보
	UPROPERTY(Transient)
	FCloneCommandType	PendingCommand;

	bool	bHasValidTarget = false;

	// 커서가 올라가 있어 테두리를 강조 중인 상호작용 액터
	TWeakObjectPtr<AActor>	HoveredActor;

	// 확정·취소 중복 처리 방지
	bool	bSelectionFinished = false;

	// Task가 추가한 입력 바인딩만 제거하기 위한 핸들
	TArray<uint32>	InputBindingHandles;

	// 초기화가 도중에 실패해도 자신이 변경한 부분만 복구
	bool	bMappingContextAdded = false;
	bool	bTimeDilationChanged = false;

	float	PreviousTimeDilation = 1.0f;
};
