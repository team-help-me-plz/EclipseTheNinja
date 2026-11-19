// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzelObject/PressureSwitch.h"
#include "Interface/Interactor.h"
#include "Interface/Activatable.h"
#include "Checkpoint/CheckpointBoundComponent.h"
#include "Component/InteractHighlightComponent.h"
#include "Save/GameSaveSubsystem.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Util/EclipseCollisionChannels.h"

namespace
{
	// 발판을 누를 수 있는 컴포넌트인지. 플레이어·분신 캡슐(Pawn)과 몬스터 캡슐(Monster)만 누른다.
	bool IsPressingComponent(const UPrimitiveComponent* Component)
	{
		if (!IsValid(Component))
		{
			return false;
		}

		const ECollisionChannel ObjectType = Component->GetCollisionObjectType();

		return ObjectType == ECC_Pawn || ObjectType == ECC_Monster;
	}
}

APressureSwitch::APressureSwitch(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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
	this->PressureTrigger->SetCollisionResponseToChannel(ECC_Monster, ECR_Overlap);
	this->PressureTrigger->SetGenerateOverlapEvents(true);
	SetRootComponent(this->PressureTrigger);

	this->MeshAssembly = CreateDefaultSubobject<UChildActorComponent>(TEXT("MeshAssembly"));
	this->MeshAssembly->SetMobility(EComponentMobility::Movable);
	this->MeshAssembly->SetupAttachment(this->PressureTrigger);

	this->CheckpointBound = CreateDefaultSubobject<UCheckpointBoundComponent>(TEXT("CheckpointBound"));
	// 플레이어 감지 함정처럼 테두리가 필요 없는 자식이 DoNotCreateDefaultSubobject로 뺄 수 있게 선택 컴포넌트로 만든다
	this->InteractHighlight = CreateOptionalDefaultSubobject<UInteractHighlightComponent>(TEXT("InteractHighlight"));

	// 발판 동작 기본 사운드(에셋 시트). BP에서 바꿀 수 있다
	static ConstructorHelpers::FObjectFinder<USoundBase> PressSoundFinder(
		TEXT("/Game/Audio/footstep_on_wooden_floor_38.footstep_on_wooden_floor_38"));

	if (PressSoundFinder.Succeeded())
	{
		this->PressSound = PressSoundFinder.Object;
	}
}

void APressureSwitch::ResetToInitial_Implementation()
{
	this->bIsInteracted = false;
	// 대상도 처음 상태로 돌아가므로 눌림 상태를 비운다. 리스폰으로 내려간 캐릭터는 다시 밟으면 눌린다
	this->bPressedActive = false;
	this->PressingComponents.Reset();
	this->RefreshInteractState();
}

void APressureSwitch::BeginPlay()
{
	Super::BeginPlay();

	// 이어하기라면 이미 눌린 한 번용 발판을 다시 작동하지 않게 한다
	UGameSaveSubsystem* Save = UGameSaveSubsystem::Get(this);
	FSavedActorState State;

	if (Save && Save->TakeActorState(this, State))
	{
		this->bIsInteracted = State.bIsUsed;
	}

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

void APressureSwitch::PostLoad()
{
	Super::PostLoad();

	// 구버전 단일 대상(TargetActor)으로 저장된 레벨을 배열로 옮긴다
	if (this->TargetActor)
	{
		this->TargetActors.AddUnique(this->TargetActor);
		this->TargetActor = nullptr;
	}
}

bool APressureSwitch::HasActivatableTarget() const
{
	for (const AActor* Target : this->TargetActors)
	{
		if (IsValid(Target) && Target->Implements<UActivatable>())
		{
			return (true);
		}
	}
	return (false);
}

void APressureSwitch::ToggleTargets()
{
	for (AActor* Target : this->TargetActors)
	{
		if (IsValid(Target) && Target->Implements<UActivatable>())
		{
			IActivatable::Execute_ToggleActivate(Target);
		}
	}
}

void APressureSwitch::SetTargetIndicator(bool bShow)
{
	for (AActor* Target : this->TargetActors)
	{
		if (!IsValid(Target) || !Target->Implements<UActivatable>())
		{
			continue ;
		}
		if (bShow)
		{
			IActivatable::Execute_ShowActivationIndicator(Target);
		}
		else
		{
			IActivatable::Execute_HideActivationIndicator(Target);
		}
	}
}

void APressureSwitch::OnPressureBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 발판은 Pawn(캡슐 등)이 밟았을 때만 작동한다. InteractBox 같은 WorldDynamic 겹침은 무시
	if (!IsPressingComponent(OtherComp))
	{
		return ;
	}
	// 이미 눌려 있으면 다시 작동하지 않는다. 플레이어와 분신이 함께 올라가도 한 번만 눌린다
	this->PressingComponents.Add(OtherComp);
	this->CountPressingComponents();
	if (this->bPressedActive)
	{
		return ;
	}
	// 한 번만 눌리는 발판이고 이미 눌렸을 때 아무것도 하지 않는다
	if (!this->bIsUnpressure && !this->bIsReinteractable && this->bIsInteracted)
	{
		return ;
	}
	// 발판을 작동시키는 것은 플레이어·분신(Interactor)이다. 몬스터는 올라가 있어도 무게만 더한다
	if (!IsValid(OtherActor) || !OtherActor->Implements<UInteractor>() || !this->HasActivatableTarget())
	{
		return ;
	}
	IInteractable::Execute_Interact(this, OtherActor);
	this->bPressedActive = true;
}

void APressureSwitch::OnPressureEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!IsPressingComponent(OtherComp))
	{
		return ;
	}
	this->PressingComponents.Remove(OtherComp);
	// 아직 누군가 올라가 있으면 그대로 눌린 상태를 유지한다
	if (this->CountPressingComponents() > 0 || !this->bPressedActive)
	{
		return ;
	}
	this->bPressedActive = false;
	if (!this->bIsUnpressure)
	{
		return;
	}
	// 처음 누른 쪽이 먼저 내려가고 다른 캐릭터가 마지막으로 내려가도, 모두 내려간 순간 대상을 되돌린다
	this->ToggleTargets();
	this->RefreshInteractState();
}

int32 APressureSwitch::CountPressingComponents()
{
	for (auto It = this->PressingComponents.CreateIterator(); It; ++It)
	{
		const UPrimitiveComponent* Component = It->Get();
		if (!IsValid(Component) || !IsValid(Component->GetOwner()) || Component->GetOwner()->IsActorBeingDestroyed()
			|| !Component->IsCollisionEnabled())
		{
			It.RemoveCurrent();
		}
	}
	return (this->PressingComponents.Num());
}

void APressureSwitch::Interact_Implementation(AActor* InActor)
{
	if (!IsValid(InActor) || !InActor->Implements<UInteractor>())
	{
		return;
	}
	if (!this->HasActivatableTarget())
	{
		return;
	}
	this->ToggleTargets();
	this->bIsInteracted = true;
	this->RefreshInteractState();
	if (this->PressSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, this->PressSound, GetActorLocation());
	}

	// 누르는 동안만 작동하거나 여러 번 누를 수 있는 발판은 남길 상태가 없다. 한 번용 발판만 기록한다
	UGameSaveSubsystem* Save = UGameSaveSubsystem::Get(this);
	if (Save && !this->bIsUnpressure && !this->bIsReinteractable)
	{
		FSavedActorState State;
		State.bIsUsed = true;
		Save->RecordActorState(this, State);
	}
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
	this->SetTargetIndicator(bShouldShowTarget);
}





