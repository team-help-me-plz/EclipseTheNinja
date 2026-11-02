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
class UAnimMontage;
class UInteractHighlightComponent;
class UWidgetComponent;

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
	virtual EInteractState	GetInteractState_Implementation() override;

	virtual void	RegisterSafePoint_Implementation(const ATargetPoint* InSafePoint) override;
	virtual void	ReturnToSafePoint_Implementation() override;

	// 수명이 끝나면 Destroy 대신 풀에 반환 (풀이 등록되지 않았으면 Destroy)
	virtual void	LifeSpanExpired() override;

	void	StopCloneMovement();

	// 오브젝트 풀: 스폰 위치 기준 이동 상태 초기화 / 명령, 쿨다운, 안전 지점 정리
	virtual void	OnSpawnFromPool_Implementation() override;
	virtual void	OnReturnToPool_Implementation() override;

	// 분신이 사라질 때(풀 반환 또는 파괴) 한 번 호출된다. 풀링되면 OnDestroyed가 호출되지 않으므로 이쪽을 사용할 것
	UPROPERTY(BlueprintAssignable, Category = "Clone")
	FOnCloneDespawned	OnCloneDespawned;

	// 풀에 반환되어 대기 중이면 false. 풀링된 분신은 파괴되지 않으므로 유효성 검사만으로는 구분할 수 없다.
	bool	IsSpawned() const { return (this->bIsSpawned); }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Command|Margin")
	float	TargetLocationMargin = 49.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact|Margin")
	FVector	InteractVelocity = FVector(0.f, 0.f, 500.f);

	// 상호작용 명령으로 대상에 도착했을 때 재생하는 몽타주. 새 명령을 받거나 풀에 반환되면 중단된다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact|Montage")
	TObjectPtr<UAnimMontage>	InteractMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact|Montage", meta = (ClampMin = "0.0"))
	float	InteractMontageBlendOutTime = 0.25f;

	// 상호작용 명령으로 대상에 도착했을 때 재생하는 몽타주. 새 명령을 받거나 풀에 반환되면 중단된다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact|Montage")
	TObjectPtr<UAnimMontage>	InteractPlayerMontage = nullptr;

	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact|Montage", meta = (ClampMin = "0.0"))
	//float	InteractMontageBlendOutTime = 0.25f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInteractHighlightComponent>	InteractHighlight = nullptr;

	// 선택되어 E키로 상호작용할 수 있을 때만 보이는 키 위젯. 위치와 위젯 클래스는 블루프린트에서 지정
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent>	InteractKeyWidget = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Command|Move")
	float	TurnPauseTime = 0.2f;

	// 목표 지점 근처에서 좌우 왕복이 이 시간 이상 이어지면 도달 불가로 판정하고 멈춘다 (높이가 다른 목표 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Command|Move", meta = (ClampMin = "0.0"))
	float	UnreachableOscillationTime = 0.5f;

	void	OnInteractPlayerMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
public:
	// 마지막 명령의 목표 지점이 도달 불가로 판정되었는지
	bool	IsTargetUnreachable() const { return (this->bIsTargetUnreachable); }

private:
	float	TurnPauseRemaining = 0.f;

	// 이동 중 방향을 뒤집은 횟수. 두 번째부터는 목표를 지나쳐 되돌아가는 왕복으로 본다
	int32	WalkDirFlipCount = 0;
	float	OscillationElapsed = 0.f;
	bool	bIsTargetUnreachable = false;

	void	ResetMoveProgress();
	void	ResetInteractionCooldown();

	void	PlayInteractMontage();
	// 몽타주의 블렌드 아웃이 시작될 때 TargetPlayer를 날린다. 몽타주가 없거나 재생에 실패하면 즉시 날린다
	void	PlayInteractPlayerMontage();
	// TargetPlayer를 날리고 상호작용 쿨다운 타이머를 시작한다
	void	LaunchTargetPlayer();
	// 상호작용 중 막아 둔 TargetPlayer의 입력을 돌려주고 비운다. 여러 번 호출해도 한 번만 처리된다
	void	ReleaseTargetPlayer();
	void	StopTargetMontage(UAnimMontage* InMontage);

	// 대상이 등 뒤에 있으면 대상 쪽으로 즉시 돌아선다
	void	FaceToward(const AActor* InActor);

	// 현재 플래그로 InteractState를 다시 계산하고 하이라이트를 갱신
	void	RefreshInteractState();
	void	RefreshHighlight();

	// 현재 위치와 방향을 기준으로 이동 평면과 걷는 방향을 설정
	void	InitializeWalkState();

	// 사용 중일 때만 OnCloneDespawned를 한 번 브로드캐스트
	void	NotifyDespawned();

	bool	bIsSpawned = false;

	bool	bIsArrived = false;
	// 플레이어와 상호작용한 뒤에는 true. 풀에 반환될 때까지 SetCommand를 무시한다
	bool	bIsCommandLocked = false;
	FVector	WalkDir = FVector::ZeroVector;

	TObjectPtr<AActor>	TargetActor = nullptr;
	FVector				TargetLocation = FVector::ZeroVector;
	ECloneCommandType	CommandType;

	FTimerHandle	InteractionCooldownTimer;
	bool			bIsInteractCooldown = false;
	float			InteractInterval = 0.5f;

	EInteractState	InteractState = EInteractState::Ready;
	// 선택(강조)을 요청한 수. InteractBox 선택, 발판 강조, 분신 조준 커서가 각각 Show/Hide를 짝지어 호출한다
	int32			SelectCount = 0;

	FVector	SafePointLocation = FVector::ZeroVector;
	FRotator	SafePointRotate = FRotator::ZeroRotator;

	TWeakObjectPtr<ACharacter>	TargetPlayer = nullptr;
};
