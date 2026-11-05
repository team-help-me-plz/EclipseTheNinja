


#include "Component/CuttableComponent.h"
#include "Interface/Interactor.h"
#include "Interface/Poolable.h"
#include "ObjectPool/ObjectPoolSubsystem.h"
#include "Util/EclipseCollisionChannels.h"

#include "GameFramework/Pawn.h"
#include "TimerManager.h"

UCuttableComponent::UCuttableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 표창(WorldDynamic)에만 막히고, 캐릭터는 통과한다
	this->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	this->SetCollisionResponseToAllChannels(ECR_Ignore);
	this->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	this->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	this->SetCollisionResponseToChannel(ECC_Monster, ECR_Ignore);
	this->CanCharacterStepUpOn = ECB_No;
}

void UCuttableComponent::BeginPlay()
{
	Super::BeginPlay();

	this->OnComponentHit.AddDynamic(this, &UCuttableComponent::OnRopeHit);
}

void UCuttableComponent::OnRopeHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (this->bIsCutted || !IsValid(OtherActor))
	{
		return ;
	}
	// 플레이어나 분신(IInteractor)이 던진 것만 줄을 끊는다. 적이 던진 표창은 무시
	const APawn*	ThrowerPawn = OtherActor->GetInstigator();
	if (!ThrowerPawn || !ThrowerPawn->Implements<UInteractor>())
	{
		return ;
	}
	this->CutRope();
}

void UCuttableComponent::CutRope()
{
	if (this->bIsCutted)
	{
		return ;
	}
	this->bIsCutted = true;
	this->SetVisibility(false);
	this->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	this->ReleaseAttachedActors();
	this->OnRopeCut.Broadcast();
}

void UCuttableComponent::ReleaseAttachedActors()
{
	TArray<TWeakObjectPtr<AActor>>	AttachedActors;
	for (const USceneComponent* Child : this->GetAttachChildren())
	{
		AActor*	ChildOwner = Child ? Child->GetOwner() : nullptr;
		if (ChildOwner && ChildOwner != this->GetOwner() && ChildOwner->Implements<UPoolable>())
		{
			AttachedActors.AddUnique(ChildOwner);
		}
	}
	if (AttachedActors.IsEmpty() || !this->GetWorld())
	{
		return ;
	}
	// 표창의 충돌 처리 도중에 끊길 수 있으므로 다음 틱에 반환한다
	this->GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
		[AttachedActors]()
		{
			for (const TWeakObjectPtr<AActor>& AttachedActor : AttachedActors)
			{
				if (AttachedActor.IsValid())
				{
					UObjectPoolSubsystem::Release(AttachedActor.Get());
				}
			}
		}));
}

void UCuttableComponent::RestoreRope()
{
	this->bIsCutted = false;
	this->SetVisibility(true);
	this->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void UCuttableComponent::SetRopeLength(const float InLength)
{
}
