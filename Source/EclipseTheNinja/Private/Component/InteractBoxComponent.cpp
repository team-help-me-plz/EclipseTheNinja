


#include "Component/InteractBoxComponent.h"
#include "Interface/Interactable.h"

void UInteractBoxComponent::BeginPlay()
{
	Super::BeginPlay();

	this->OnComponentBeginOverlap.AddDynamic(this, &UInteractBoxComponent::OnInteractBoxBeginOverlap);
	this->OnComponentEndOverlap.AddDynamic(this, &UInteractBoxComponent::OnInteractBoxEndOverlap);
}

void UInteractBoxComponent::OnScrollInput(const int32 Direction)
{
	if (this->TargetActor.IsValid())
	{
		int32		NextIndex = this->OverlappedInteractActor.Find(this->TargetActor.Get());
		const int32	ArraySize = this->OverlappedInteractActor.Num();

		NextIndex = (NextIndex + Direction + ArraySize) % ArraySize;
		this->ChangeTargetActor(this->FindNextActor(NextIndex));
	}
}

void UInteractBoxComponent::TryInteract()
{
	if (this->TargetActor.IsValid())
	{
		IInteractable::Execute_Interact(this->TargetActor.Get(), GetOwner());
	}
}

void UInteractBoxComponent::OnInteractBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor == GetOwner())
	{
		return;
	}
	AActor*	InteractActor = this->ResolveInteractableActor(OtherActor);
	if (!IsValid(InteractActor))
	{
		return;
	}
	if (IInteractable::Execute_bIsInteractReady(InteractActor))
	{
		this->OverlappedInteractActor.AddUnique(InteractActor);
		if (!this->TargetActor.IsValid())
		{
			this->ChangeTargetActor(InteractActor);
		}
	}
	else if (IInteractable::Execute_GetInteractState(InteractActor) == EInteractState::Ready
		&& !this->HighlightOnlyActors.Contains(InteractActor))
	{
		// 상호작용 키로는 선택되지 않지만 작동 가능한 액터(발판 등)는 목록에 넣지 않고 테두리만 강조한다
		this->HighlightOnlyActors.Add(InteractActor);
		IInteractable::Execute_ShowInteractionIndicator(InteractActor);
	}
}

void UInteractBoxComponent::OnInteractBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor == GetOwner())
	{
		return ;
	}
	// 파괴 중인 액터도 목록에서 빠져야 하므로 IsValid로 거르지 않는다
	AActor*	InteractActor = this->ResolveInteractableActor(OtherActor);
	if (InteractActor == nullptr)
	{
		return ;
	}
	if (IsOverlappingActor(OtherActor) || IsOverlappingActor(InteractActor))
	{
		return ;
	}
	const int32	OverlappedIndex = this->OverlappedInteractActor.Find(InteractActor);
	if (OverlappedIndex == INDEX_NONE)
	{
		// 목록에 없는 액터는 테두리만 강조했던 경우에만 되돌린다
		if (this->HighlightOnlyActors.Remove(InteractActor) > 0 && IsValid(InteractActor))
		{
			IInteractable::Execute_HideInteractionIndicator(InteractActor);
		}
		return ;
	}
	this->OverlappedInteractActor.RemoveAt(OverlappedIndex);
	if (this->TargetActor.Get() == InteractActor || !TargetActor.IsValid())
	{
		this->ChangeTargetActor(this->FindNextActor(OverlappedIndex));
	}
}

void UInteractBoxComponent::ChangeTargetActor(AActor* InActor)
{
	if (this->TargetActor.IsValid())
	{
		IInteractable::Execute_HideInteractionIndicator(this->TargetActor.Get());
	}
	this->TargetActor = InActor;
	if (this->TargetActor.IsValid())
	{
		IInteractable::Execute_ShowInteractionIndicator(this->TargetActor.Get());
	}
}

AActor* UInteractBoxComponent::FindNextActor(const int32 InIndex)
{
	const int32	OverlappedCount = this->OverlappedInteractActor.Num();
	if (OverlappedCount == 0)
	{
		return (nullptr);
	}
	const int32	NextIndex = InIndex % OverlappedCount;
	return (this->OverlappedInteractActor[NextIndex].Get());
}

AActor* UInteractBoxComponent::ResolveInteractableActor(AActor* InActor) const
{
	for (AActor* Current = InActor; Current != nullptr; Current = Current->GetParentActor())
	{
		if (Current->Implements<UInteractable>())
		{
			return (Current);
		}
	}
	return (nullptr);
}

