


#include "Component/InteractHighlightComponent.h"
#include "Component/ActivateHighlightComponent.h"

#include "Components/MeshComponent.h"
#include "Materials/MaterialInstance.h"
#include "UObject/ConstructorHelpers.h"

const FName UInteractHighlightComponent::NoHighlightTag(TEXT("NoHighlight"));

// Sets default values for this component's properties
UInteractHighlightComponent::UInteractHighlightComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// 모든 상호작용 액터가 같은 테두리를 쓰도록 기본 MI를 지정한다 (Blueprint에서 덮어쓸 수 있음)
	static ConstructorHelpers::FObjectFinder<UMaterialInstance> DefaultMI(
		TEXT("/Game/Materials/InteractSelect/MI_InteractOutline_Default.MI_InteractOutline_Default"));
	static ConstructorHelpers::FObjectFinder<UMaterialInstance> SelectedMI(
		TEXT("/Game/Materials/InteractSelect/MI_InteractOutline_Selected.MI_InteractOutline_Selected"));
	static ConstructorHelpers::FObjectFinder<UMaterialInstance> CooldownMI(
		TEXT("/Game/Materials/InteractSelect/MI_InteractOutline_CooldownSelected.MI_InteractOutline_CooldownSelected"));

	if (DefaultMI.Succeeded())
	{
		this->DefaultHighlight = DefaultMI.Object;
	}
	if (SelectedMI.Succeeded())
	{
		this->SelectedHighlight = SelectedMI.Object;
	}
	if (CooldownMI.Succeeded())
	{
		this->CooldownHighlight = CooldownMI.Object;
	}
}

void UInteractHighlightComponent::SetHighlight(EInteractState InState, bool InbIsSelected)
{
	if (InState == EInteractState::Done)
	{
		this->SetParentHighlight(nullptr);
		return ;
	}
	if (InbIsSelected)
	{
		switch (InState)
		{
		case EInteractState::Ready:
		{
			this->SetParentHighlight(this->SelectedHighlight);
			return ;
		}
		case EInteractState::Cooldown:
		{
			this->SetParentHighlight(this->CooldownHighlight);
			return ;
		}
		default:
		{
			this->SetParentHighlight(nullptr);
			return ;
		}
		}
	}
	this->SetParentHighlight(this->DefaultHighlight);
}


// Called when the game starts
void UInteractHighlightComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	this->SetParentHighlight(this->DefaultHighlight);
}


// Called every frame
void UInteractHighlightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UInteractHighlightComponent::SetParentHighlight(UMaterialInstance* InMI)
{
	this->CurrentHighlight = InMI;

	// Activate 테두리가 켜져 있으면 덮어쓰지 않고, 꺼질 때 ReapplyHighlight로 복원한다
	const UActivateHighlightComponent*	ActivateHighlight = GetOwner()->FindComponentByClass<UActivateHighlightComponent>();
	if (IsValid(ActivateHighlight) && ActivateHighlight->IsHighlighted())
	{
		return ;
	}
	ApplyOverlay(GetOwner(), InMI);
}

void UInteractHighlightComponent::ReapplyHighlight()
{
	ApplyOverlay(GetOwner(), this->CurrentHighlight);
}

void UInteractHighlightComponent::ApplyOverlay(AActor* InOwner, UMaterialInstance* InMI)
{
	if (!IsValid(InOwner))
	{
		return ;
	}
	TArray<UMeshComponent*> Meshes;
	InOwner->GetComponents<UMeshComponent>(Meshes, true);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (Mesh->ComponentHasTag(NoHighlightTag))
		{
			continue ;
		}
		Mesh->SetOverlayMaterial(InMI);
	}
}

