#pragma once

#include "CoreMinimal.h"
#include "Interface/Interactable.h"

#include "GameFramework/Actor.h"
#include "VentPassage.generated.h"

class UAnimMontage;
class UBoxComponent;
class UInteractHighlightComponent;
class UWidgetComponent;
class ACharacter;

UCLASS()
class ECLIPSETHENINJA_API AVentPassage : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AVentPassage();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void			Interact_Implementation(AActor* InActor) override;
	virtual void			ShowInteractionIndicator_Implementation() override;
	virtual void			HideInteractionIndicator_Implementation() override;
	virtual bool			bIsInteractReady_Implementation() override;
	virtual EInteractState	GetInteractState_Implementation() override;
	// 분신은 벤트를 지나가지 않는다
	virtual bool			CanCloneInteract_Implementation() override { return (false); }

	AVentPassage*	GetTargetVent() const { return (this->TargetVent); }

protected:
	virtual void	ResetInteractionCooldown();

	// 현재 플래그로 InteractState를 다시 계산하고 하이라이트를 갱신
	void	RefreshInteractState();
	void	RefreshHighlight();

	// 반대쪽 벤트가 선택되었을 때 함께 강조. 받은 강조는 다시 반대쪽으로 넘기지 않는다
	void	SetLinkedSelected(bool InbIsLinkedSelected);

	// 쿨다운을 시작하고 상태를 갱신. 들어가고 나오는 동안 양쪽 벤트를 다시 쓰지 못하게 한다
	void	StartInteractCooldown();

	// TargetVent에서 InCharacter를 EntryPoint로 옮기고 EnterMontage를 역재생해 내보낸다
	void	OutVent(ACharacter* InCharacter);

	// InCharacter를 이 벤트의 EntryPoint 위치와 방향으로 순간이동
	void	AlignToEntryPoint(ACharacter* InCharacter) const;

	// 들어가는 몽타주가 블렌드 아웃을 시작하면 TargetVent로 옮긴다. 끊긴 경우에는 잠금만 푼다
	void	OnEnterMontageBlendingOut(UAnimMontage* InMontage, bool bInterrupted, TWeakObjectPtr<ACharacter> InCharacter);
	// 나오는 몽타주가 끝나면 잠금을 푼다
	void	OnExitMontageEnded(UAnimMontage* InMontage, bool bInterrupted, TWeakObjectPtr<ACharacter> InCharacter);

	// 이동하는 동안 InCharacter의 이동과 입력을 막는다
	static void	LockTraveler(ACharacter* InCharacter);
	static void	UnlockTraveler(ACharacter* InCharacter);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent>			RootScene = nullptr;

	// 들어가기 전에 서는 위치와 나온 뒤 서 있는 위치(발 기준)와 방향. 레벨에 맞게 블루프린트에서 옮긴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent>			EntryPoint = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UChildActorComponent>	MeshAssembly = nullptr;

	//// 상호작용 감지(InteractBox 오버랩, 분신 조준 Visibility 트레이스)용 판정
	//UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	//TObjectPtr<UBoxComponent>			InteractCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInteractHighlightComponent>	InteractHighlight = nullptr;

	// 선택되어 E키로 상호작용할 수 있을 때만 보이는 키 위젯. 위치와 위젯 클래스는 블루프린트에서 지정
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent>		InteractKeyWidget = nullptr;

	// 이 환풍구와 연결된 반대쪽 환풍구. 양쪽 인스턴스에서 서로를 지정
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interact|Target")
	TObjectPtr<AVentPassage>			TargetVent = nullptr;

	// 이 환풍구로 들어갈 때 재생하는 전용 몽타주. 이 환풍구로 나올 때는 같은 몽타주를 역재생한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact|Montage")
	TObjectPtr<UAnimMontage>			EnterMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact|Cooldown", meta = (ClampMin = "0.05"))
	float	InteractInterval = 1.5f;

	FTimerHandle	InteractionCooldownTimer;
	bool			bIsInteractCooldown = false;

private:
	EInteractState	InteractState = EInteractState::Ready;

	// 선택(강조)을 요청한 수. InteractBox 선택, 분신 조준 커서가 각각 Show/Hide를 짝지어 호출한다
	int32			SelectCount = 0;
	// 반대쪽 벤트가 선택되어 함께 강조 중인가
	bool			bIsLinkedSelected = false;
	// 반대쪽 벤트의 강조를 켜 둔 상태인가 (켜기/끄기를 짝지어 호출하기 위함)
	bool			bIsShowingLinkedTarget = false;
};
