// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzelObject/PressureSwitch.h"
#include "Interface/Interactor.h"
#include "Checkpoint/CheckpointBoundComponent.h"
#include "Component/InteractHighlightComponent.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

APressureSwitch::APressureSwitch()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	this->PressureTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("PressureTrigger"));

	// 발판 메시의 스케일에 따라 박스 크기가 찌그러지지 않도록 설정
	this->PressureTrigger->SetAbsolute(false, false, true);

	// Extent는 전체 크기의 절반: 실제 크기는 150 × 150 × 100
	this->PressureTrigger->SetBoxExtent(FVector(75.0f, 75.0f, 50.0f));

	this->PressureTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	this->PressureTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	this->PressureTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	this->PressureTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	this->PressureTrigger->SetGenerateOverlapEvents(true);
	SetRootComponent(this->PressureTrigger);

	this->MeshAssembly = CreateDefaultSubobject<UChildActorComponent>(TEXT("MeshAssembly"));
	this->MeshAssembly->SetMobility(EComponentMobility::Movable);
	this->MeshAssembly->SetupAttachment(this->PressureTrigger);

	this->CheckpointBound = CreateDefaultSubobject<UCheckpointBoundComponent>(TEXT("CheckpointBound"));
	this->InteractHighlight = CreateDefaultSubobject<UInteractHighlightComponent>(TEXT("InteractHighlight"));
}

void APressureSwitch::ResetToInitial_Implementation()
{
	this->bIsInteracted = false;
	this->RefreshInteractState();
}

void APressureSwitch::BeginPlay()
{
	Super::BeginPlay();

	this->RefreshInteractState();
}

void APressureSwitch::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void APressureSwitch::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 플레이어의 InteractBox(WorldDynamic)가 발판을 감지해 테두리를 켤 수 있도록 겹침을 허용한다
	// Blueprint에 저장된 콜리전 설정보다 우선하도록 런타임에 지정한다
	this->PressureTrigger->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);

	this->PressureTrigger->OnComponentBeginOverlap.AddDynamic(this, &APressureSwitch::OnPressureBeginOverlap);
	this->PressureTrigger->OnComponentEndOverlap.AddDynamic(this, &APressureSwitch::OnPressureEndOverlap);
}

void APressureSwitch::OnPressureBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 발판은 Pawn(캡슐 등)이 밟았을 때만 작동한다. InteractBox 같은 WorldDynamic 겹침은 무시
	if (!IsValid(OtherComp) || OtherComp->GetCollisionObjectType() != ECC_Pawn)
	{
		return ;
	}
	// 한 번만 눌리는 발판이고 이미 눌렸을 때 아무것도 하지 않는다
	if (!this->bIsUnpressure && !this->bIsReinteractable && this->bIsInteracted)
	{
		return ;
	}
	IInteractable::Execute_Interact(this, OtherActor);
}

void APressureSwitch::OnPressureEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!IsValid(OtherComp) || OtherComp->GetCollisionObjectType() != ECC_Pawn)
	{
		return ;
	}
	if (!this->bIsUnpressure)
	{
		return;
	}
	IInteractable::Execute_Interact(this, OtherActor);
}

void APressureSwitch::Interact_Implementation(AActor* InActor)
{
	if (!IsValid(InActor) || !InActor->Implements<UInteractor>())
	{
		return;
	}
	if (!this->TargetActor || !this->TargetActor->Implements<UActivatable>())
	{
		return;
	}
	IActivatable::Execute_ToggleActivate(this->TargetActor);
	this->bIsInteracted = true;
	this->RefreshInteractState();
}
void APressureSwitch::ShowInteractionIndicator_Implementation()
{
	this->SelectCount++;
	this->RefreshHighlight();
}

void APressureSwitch::HideInteractionIndicator_Implementation()
{
	this->SelectCount = FMath::Max(0, this->SelectCount - 1);
	this->RefreshHighlight();
}

bool APressureSwitch::bIsInteractReady_Implementation()
{
	// 발판은 밟아서 작동하므로 상호작용 키로는 선택되지 않는다
	return (false);
}

EInteractState APressureSwitch::GetInteractState_Implementation()
{
	return (this->InteractState);
}

void APressureSwitch::RefreshInteractState()
{
	// OnPressureBeginOverlap에서 더 이상 작동하지 않는 조건과 같다
	if (!this->bIsUnpressure && !this->bIsReinteractable && this->bIsInteracted)
	{
		this->InteractState = EInteractState::Done;
	}
	else
	{
		this->InteractState = EInteractState::Ready;
	}
	this->RefreshHighlight();
}

void APressureSwitch::RefreshHighlight()
{
	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetHighlight(this->InteractState, (this->SelectCount > 0));
	}

	// 강조되어 있는 동안 이 발판이 작동시킬 대상의 테두리도 켠다
	const bool	bShouldShowTarget = (this->SelectCount > 0) && this->InteractState != EInteractState::Done;
	if (bShouldShowTarget == this->bIsShowingTargetIndicator)
	{
		return ;
	}
	this->bIsShowingTargetIndicator = bShouldShowTarget;
	if (!IsValid(this->TargetActor) || !this->TargetActor->Implements<UActivatable>())
	{
		return ;
	}
	if (bShouldShowTarget)
	{
		IActivatable::Execute_ShowActivationIndicator(this->TargetActor);
	}
	else
	{
		IActivatable::Execute_HideActivationIndicator(this->TargetActor);
	}
}





