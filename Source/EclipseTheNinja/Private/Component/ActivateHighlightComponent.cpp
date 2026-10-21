


#include "Component/ActivateHighlightComponent.h"
#include "Component/InteractHighlightComponent.h"

#include "Materials/MaterialInstance.h"
#include "UObject/ConstructorHelpers.h"

UActivateHighlightComponent::UActivateHighlightComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInstance> ActivateMI(
		TEXT("/Game/Blueprint/Week_2/InteractSelect/MI_InteractOutline_Selected.MI_InteractOutline_Selected"));

	if (ActivateMI.Succeeded())
	{
		this->ActivateHighlight = ActivateMI.Object;
	}
}

void UActivateHighlightComponent::ShowHighlight()
{
	this->ShowCount++;
	if (this->ShowCount == 1)
	{
		UInteractHighlightComponent::ApplyOverlay(GetOwner(), this->ActivateHighlight);
	}
}

void UActivateHighlightComponent::HideHighlight()
{
	if (this->ShowCount == 0)
	{
		return ;
	}
	this->ShowCount--;
	if (this->ShowCount > 0)
	{
		return ;
	}
	// 같은 액터가 상호작용 테두리도 가지고 있다면(문 등) 그 테두리로 되돌린다
	UInteractHighlightComponent*	InteractHighlight = GetOwner()->FindComponentByClass<UInteractHighlightComponent>();
	if (IsValid(InteractHighlight))
	{
		InteractHighlight->ReapplyHighlight();
		return ;
	}
	UInteractHighlightComponent::ApplyOverlay(GetOwner(), nullptr);
}
