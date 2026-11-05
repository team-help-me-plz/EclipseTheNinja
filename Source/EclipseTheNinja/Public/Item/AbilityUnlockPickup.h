#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/Interactable.h"
#include "GAS/AbilityInfo.h"
#include "AbilityUnlockPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UInteractHighlightComponent;
class UWidgetComponent;
class UTexture2D;
class UAbilityUnlockNoticeWidget;
class APlayerCharacter;

/**
 * 상호작용하면 지정한 어빌리티를 해금하는 액터. 한 번 해금하면 다시 상호작용할 수 없다.
 * 해금 정보는 세이브에 기록되므로, 이어하기나 맵을 다시 열어도 이미 해금한 액터는 나타나지 않는다.
 * 체크포인트 리셋 대상이 아니므로 사망 후 리스폰해도 되살아나지 않는다.
 * 실제로 부여할 어빌리티 클래스는 플레이어 BP의 UnlockableAbilities에서 같은 InputID로 찾는다.
 */
UCLASS()
class ECLIPSETHENINJA_API AAbilityUnlockPickup : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AAbilityUnlockPickup();

	virtual void	Interact_Implementation(AActor* InActor) override;
	virtual void	ShowInteractionIndicator_Implementation() override;
	virtual void	HideInteractionIndicator_Implementation() override;
	virtual bool	bIsInteractReady_Implementation() override;
	virtual EInteractState	GetInteractState_Implementation() override;
	// 분신은 어빌리티를 대신 얻을 수 없다
	virtual bool	CanCloneInteract_Implementation() override { return (false); }

protected:
	virtual void	BeginPlay() override;

	/** 해금 성공 직후 호출된다. 사운드, 이펙트 같은 연출을 BP에서 붙인다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AbilityUnlock")
	void	BP_OnUnlocked(APlayerCharacter* InPlayer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent>	InteractTrigger = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent>	Mesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInteractHighlightComponent>	InteractHighlight = nullptr;

	// 선택되어 E키로 상호작용할 수 있을 때만 보이는 키 위젯. 위젯 클래스는 BP에서 지정
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent>	InteractKeyWidget = nullptr;

	/** 해금할 어빌리티. 플레이어 BP의 UnlockableAbilities에 같은 InputID 항목이 있어야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityUnlock")
	EAbilityInputID	AbilityToUnlock = EAbilityInputID::None;

	/** 해금 후 액터를 숨길지. false면 메시는 남기고 상호작용만 막는다 (제단 등). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityUnlock")
	bool	bHideAfterUnlock = true;

	/** 메시 머티리얼에 넣을 스킬 텍스처. 비워 두면 머티리얼을 그대로 둔다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityUnlock|Mesh")
	TObjectPtr<UTexture2D>	SkillTexture = nullptr;

	/** 스킬 텍스처를 받을 머티리얼의 Texture 파라미터 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityUnlock|Mesh")
	FName	SkillTextureParameterName = TEXT("SkillTexture");

	/** 스킬 텍스처를 넣을 메시의 머티리얼 슬롯 번호 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityUnlock|Mesh", meta = (ClampMin = "0"))
	int32	SkillTextureMaterialIndex = 0;

	/** 해금 성공 시 띄울 안내 위젯. 비워 두면 위젯 없이 해금만 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityUnlock|Notice")
	TSubclassOf<UAbilityUnlockNoticeWidget>	NoticeWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityUnlock|Notice")
	FText	NoticeTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityUnlock|Notice", meta = (MultiLine = "true"))
	FText	NoticeDescription;

	/** 안내 위젯 아이콘. 비워 두면 SkillTexture를 대신 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityUnlock|Notice")
	TObjectPtr<UTexture2D>	NoticeIcon = nullptr;

	/** 상호작용 후 위젯을 띄우기까지의 지연(초). 상호작용 몽타주를 조금 보여 준 뒤 멈추고 싶을 때 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityUnlock|Notice", meta = (ClampMin = "0.0"))
	float	NoticeOpenDelay = 0.f;

private:
	// 어빌리티를 부여한다. 세이브 세션이 없으면(에디터에서 맵 직접 실행) 저장 없이 부여만 한다.
	bool	GrantAbility(APlayerCharacter* InPlayer) const;
	// 다시 상호작용할 수 없게 상태를 Done으로 바꾸고 콜리전을 끈다
	void	Consume();
	// 이미 얻은 능력의 픽업을 레벨에서 없앤다. bHideAfterUnlock이 false인 제단형은 남기고 상호작용만 막는다
	void	RemoveAsObtained();
	// 같은 능력을 주는 이 레벨의 다른 픽업을 모두 없앤다
	void	RemoveOtherPickups();
	void	OpenNotice();
	void	RefreshHighlight();
	// 메시 머티리얼을 다이내믹 인스턴스로 바꾸고 SkillTexture를 넣는다
	void	ApplySkillTexture();

	EInteractState	InteractState = EInteractState::Ready;
	int32	SelectCount = 0;

	TWeakObjectPtr<APlayerCharacter>	UnlockedPlayer;
	FTimerHandle	NoticeTimer;
};
