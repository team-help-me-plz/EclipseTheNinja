#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/PlayerCondition.h"
#include "HidingComponent.generated.h"

class AStealthZone;
class APlayerCharacter;
class UAnimMontage;
class UInputMappingContext;

/** 은신구역에 들어가고 나오는 진행 단계. None이 아니면 다른 행동을 막는다. */
UENUM(BlueprintType)
enum class EHidingPhase : uint8
{
	None,
	Entering,
	Hidden,
	Exiting,
};

/**
 * 은신구역에 들어가고 나오는 연출과 이동을 맡는다. 플레이어에 붙여 사용한다.
 * 몽타주는 Start → Loop(자기 반복) → End 섹션으로 구성하고, 위치는 몽타주 루트 모션 대신 코드가 옮긴다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ECLIPSETHENINJA_API UHidingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHidingComponent();

	// 은신구역이 상호작용을 받으면 호출한다. 공중이거나 전투 중이면 숨지 않고 false를 돌려준다.
	bool	TryEnter(const AStealthZone* InZone);
	// 숨은 뒤 나오기 입력으로 호출한다. 들어가거나 나오는 중에는 무시한다.
	void	RequestExit();
	// 사망, 리스폰처럼 연출을 끝까지 볼 수 없을 때 바로 원래 자리로 되돌린다.
	void	ForceExit();

	bool			IsHiding() const { return (this->Phase != EHidingPhase::None); }
	EHidingPhase	GetPhase() const { return (this->Phase); }

protected:
	virtual void BeginPlay() override;

	// +X를 바라볼 때 재생한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hiding|Montage")
	TObjectPtr<UAnimMontage>	LeftMontage = nullptr;

	// -X를 바라볼 때 재생한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hiding|Montage")
	TObjectPtr<UAnimMontage>	RightMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hiding|Montage")
	FName	StartSection = TEXT("Start");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hiding|Montage")
	FName	EndSection = TEXT("End");

	// 은신 동안 기본 IMC 대신 넣는 IMC. 나오기(상호작용), 카메라 이동, 스크롤만 넣는다.
	// 비어 있으면 입력을 잠그지 않는다(나오기 키까지 막혀 갇히지 않도록).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hiding|Input")
	TObjectPtr<UInputMappingContext>	HidingMappingContext = nullptr;

	// 몽타주나 섹션이 없을 때 들어가고 나오는 데 걸리는 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hiding|Move", meta = (ClampMin = "0.05", Units = "s"))
	float	FallbackMoveTime = 0.5f;

private:
	void	OnEnterFinished();
	void	OnExitFinished();
	// End 섹션을 재생하며 원래 이동 평면으로 걸어 나온다
	void	BeginExit();
	// 진행 중인 연출과 이동을 정리하고 평면 고정과 플레이어 상태를 원래대로 되돌린다
	void	EndHiding(bool bStopMontage);

	// 컨트롤러에 은신 입력 잠금을 걸거나 푼다
	void	SetInputLocked(bool bLocked);
	void	MoveTo(const FVector& InTarget, float InDuration);
	float	GetSectionTime(FName InSection) const;
	void	OnMontageEnded(UAnimMontage* InMontage, bool bInterrupted);
	void	OnPlayerConditionChanged(EPlayerCondition InCondition);

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage>	ActiveMontage = nullptr;

	TWeakObjectPtr<APlayerCharacter>	Player;
	EHidingPhase	Phase = EHidingPhase::None;
	FVector			HideLocation = FVector::ZeroVector;
	bool			bSavedPlaneConstraint = true;
	// 들어가는 중에 발각되면 다 들어간 뒤 바로 나온다
	bool			bExitAfterEnter = false;
	uint16			MoveSourceID = 0;
	FTimerHandle	PhaseTimer;
};
