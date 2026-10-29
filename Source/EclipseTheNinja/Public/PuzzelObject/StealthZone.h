#pragma once

#include "CoreMinimal.h"
#include "Interface/Interactable.h"

#include "GameFramework/Actor.h"
#include "StealthZone.generated.h"

class UBoxComponent;
class UInteractHighlightComponent;
class UWidgetComponent;

/**
 * 플레이어가 상호작용하면 안쪽(-Y)의 은신 지점으로 숨는 구역. 숨는 연출은 플레이어의 HidingComponent가 맡는다.
 * 숨어 있는 동안의 상호작용 입력은 플레이어가 나오기로 처리하므로 이 액터까지 오지 않는다.
 */
UCLASS()
class ECLIPSETHENINJA_API AStealthZone : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AStealthZone();

	virtual void	Interact_Implementation(AActor* InActor) override;
	virtual void	ShowInteractionIndicator_Implementation() override;
	virtual void	HideInteractionIndicator_Implementation() override;
	virtual bool	bIsInteractReady_Implementation() override;
	virtual EInteractState	GetInteractState_Implementation() override;

	// 숨었을 때 플레이어가 서 있을 위치
	FVector	GetHideLocation() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent>		RootScene = nullptr;

	// 플레이어의 상호작용 박스가 겹치는 범위. 바깥 이동 평면부터 은신 지점까지 덮도록 맞춘다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent>		InteractArea = nullptr;

	// 숨었을 때 플레이어 캡슐 중심의 X, Y. 높이는 플레이어 높이를 그대로 쓴다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent>		HidePoint = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInteractHighlightComponent>	InteractHighlight = nullptr;

	// 선택되어 E키로 상호작용할 수 있을 때만 보이는 키 위젯. 위치와 위젯 클래스는 블루프린트에서 지정
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent>	InteractKeyWidget = nullptr;

	// 발각된(전투) 상태에서 상호작용하면 띄우는 메시지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StealthZone")
	FText	CannotHideText = FText::FromString(TEXT("적에게 발각되어 숨을 수 없습니다."));

private:
	void	RefreshHighlight();

	// 선택(강조)을 요청한 수
	int32	SelectCount = 0;
};
