

#pragma once

#include "CoreMinimal.h"
#include "Interface/Interactable.h"

#include "Components/ActorComponent.h"
#include "InteractHighlightComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ECLIPSETHENINJA_API UInteractHighlightComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UInteractHighlightComponent();

	void	SetHighlight(EInteractState InState, bool InbIsSelected = false);

	// 마지막으로 지정된 상호작용 테두리를 다시 적용 (Activate 테두리가 꺼질 때 사용)
	void	ReapplyHighlight();

	// InOwner와 자식 액터의 모든 메시에 Overlay 머티리얼을 지정. NoHighlightTag가 붙은 메시는 제외한다
	static void	ApplyOverlay(AActor* InOwner, UMaterialInstance* InMI);

	// 테두리를 그리지 않을 메시에 붙이는 Component Tag
	static const FName	NoHighlightTag;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	void	SetParentHighlight(UMaterialInstance* InMI);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Highlight")
	TObjectPtr<UMaterialInstance>	DefaultHighlight = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Highlight")
	TObjectPtr<UMaterialInstance>	SelectedHighlight = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Highlight")
	TObjectPtr<UMaterialInstance>	CooldownHighlight = nullptr;

private:
	// 현재 상태에 맞는 상호작용 테두리. Activate 테두리가 켜져 있는 동안에도 갱신되어 꺼질 때 복원된다
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstance>	CurrentHighlight = nullptr;

};
