// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Clone/CloneCommandType.h"
#include "Interface/Interactable.h"
#include "Interface/Interactor.h"
#include "Interface/Poolable.h"

#include "GameFramework/Character.h"
#include "CloneCharacter.generated.h"

class ACloneCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCloneDespawned, ACloneCharacter*, Clone);

UCLASS()
class ECLIPSETHENINJA_API ACloneCharacter : public ACharacter, public IInteractor, public IInteractable, public IPoolable
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ACloneCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void	SetCommand(const FCloneCommandType& InCommand);

	virtual void	Interact_Implementation(AActor* InActor) override;
	virtual void	ShowInteractionIndicator_Implementation() override;
	virtual void	HideInteractionIndicator_Implementation() override;
	virtual bool	bIsInteractReady_Implementation() override;

	virtual void	RegisterSafePoint_Implementation(const ATargetPoint* InSafePoint) override;
	virtual void	ReturnToSafePoint_Implementation() override;

	// 수명이 끝나면 Destroy 대신 풀에 반환 (풀이 등록되지 않았으면 Destroy)
	virtual void	LifeSpanExpired() override;

	// 오브젝트 풀: 스폰 위치 기준 이동 상태 초기화 / 명령, 쿨다운, 안전 지점 정리
	virtual void	OnSpawnFromPool_Implementation() override;
	virtual void	OnReturnToPool_Implementation() override;

	// 분신이 사라질 때(풀 반환 또는 파괴) 한 번 호출된다. 풀링되면 OnDestroyed가 호출되지 않으므로 이쪽을 사용할 것
	UPROPERTY(BlueprintAssignable, Category = "Clone")
	FOnCloneDespawned	OnCloneDespawned;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Command|Margin")
	float	TargetLocationMargin = 49.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact|Margin")
	FVector	InteractVelocity = FVector(0.f, 0.f, 500.f);

private:
	void	ResetInteractionCooldown();

	// 현재 위치와 방향을 기준으로 이동 평면과 걷는 방향을 설정
	void	InitializeWalkState();

	// 사용 중일 때만 OnCloneDespawned를 한 번 브로드캐스트
	void	NotifyDespawned();

	bool	bIsSpawned = false;

	bool	bIsArrived = false;
	FVector	WalkDir = FVector::ZeroVector;

	TObjectPtr<AActor>	TargetActor = nullptr;
	FVector				TargetLocation = FVector::ZeroVector;
	ECloneCommandType	CommandType;

	FTimerHandle	InteractionCooldownTimer;
	bool			bIsInteractCooldown = false;
	float			InteractInterval = 0.5f;

	FVector	SafePointLocation = FVector::ZeroVector;
	FRotator	SafePointRotate = FRotator::ZeroRotator;
};
