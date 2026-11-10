// Fill out your copyright notice in the Description page of Project Settings.


#include "PuzzelObject/InteractSwitch.h"
#include "PuzzelObject/BaseDoor.h"
#include "Interface/Interactor.h"
#include "Interface/Activatable.h"
#include "Util/MeshAssembly.h"
#include "Checkpoint/CheckpointBoundComponent.h"
#include "Component/InteractHighlightComponent.h"
#include "Save/GameSaveSubsystem.h"

#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AInteractSwitch::AInteractSwitch()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//this->InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	//this->InteractSphere->SetSphereRadius(this->InteractRadius);
	//SetRootComponent(this->InteractSphere);

	this->RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(this->RootScene);

	this->MeshAssembly = CreateDefaultSubobject<UChildActorComponent>(TEXT("MeshAssembly"));
	this->MeshAssembly->SetupAttachment(GetRootComponent());

	this->CheckpointBound = CreateDefaultSubobject<UCheckpointBoundComponent>(TEXT("CheckpointBound"));
	this->InteractHighlight = CreateDefaultSubobject<UInteractHighlightComponent>(TEXT("InteractHighlight"));

	this->InteractKeyWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractKeyWidget"));
	this->InteractKeyWidget->SetupAttachment(GetRootComponent());
	this->InteractKeyWidget->SetWidgetSpace(EWidgetSpace::Screen);
	this->InteractKeyWidget->SetDrawAtDesiredSize(true);
	this->InteractKeyWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	this->InteractKeyWidget->SetVisibility(false);
}

void AInteractSwitch::ResetToInitial_Implementation()
{
	this->bIsInteracted = false;

	GetWorldTimerManager().ClearTimer(this->InteractionCooldownTimer);
	this->bIsInteractCooldown = false;
	this->bIsAutoReturnPending = false;
	this->RefreshInteractState();
}

// Called when the game starts or when spawned
void AInteractSwitch::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetKeyWidget(this->InteractKeyWidget);
	}
	this->RefreshInteractState();

	// 아래 MeshAssembly가 없으면 일찍 끝나므로 저장 상태는 그 전에 적용한다
	UGameInstance* GameInstance = GetGameInstance();
	UGameSaveSubsystem* Save = GameInstance
		? GameInstance->GetSubsystem<UGameSaveSubsystem>()
		: nullptr;
	FSavedActorState State;

	if (Save && Save->TakeActorState(this, State))
	{
		this->ApplySaveState(State);
	}

	AActor* AssemblyActor = this->MeshAssembly->GetChildActor();
	if (!IsValid(AssemblyActor))
	{
		return;
	}

	TArray<UStaticMeshComponent*> Meshes;
	AssemblyActor->GetComponents<UStaticMeshComponent>(Meshes);

	for (UStaticMeshComponent* Mesh : Meshes)
	{
		Mesh->SetCollisionObjectType(ECC_WorldDynamic);

		Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
}

void AInteractSwitch::PostLoad()
{
	Super::PostLoad();

	// 구버전 단일 대상(TargetActor)으로 저장된 레벨을 배열로 옮긴다
	if (this->TargetActor)
	{
		this->TargetActors.AddUnique(this->TargetActor);
		this->TargetActor = nullptr;
	}

#if WITH_EDITORONLY_DATA
	// 문에 있던 자동 닫힘(AutoCloseDelay)을 이 스위치의 AutoReinteractTime으로 옮긴다.
	// 같은 맵 패키지의 액터는 PostLoad 전에 모두 읽혀 있으므로 문 값을 그대로 읽을 수 있다
	if (this->AutoReinteractTime <= 0.0f)
	{
		for (const AActor* Target : this->TargetActors)
		{
			const ABaseDoor*	Door = Cast<ABaseDoor>(Target);
			if (Door && Door->GetDeprecatedAutoCloseDelay() > 0.0f)
			{
				this->AutoReinteractTime = FMath::Max(this->AutoReinteractTime, Door->GetDeprecatedAutoCloseDelay());
			}
		}
		if (this->AutoReinteractTime > 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] 문 AutoCloseDelay를 스위치 AutoReinteractTime %.2f초로 옮겼습니다. 맵을 저장하세요."),
				*GetName(), this->AutoReinteractTime);
		}
	}
#endif
}

bool AInteractSwitch::HasActivatableTarget() const
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

void AInteractSwitch::ToggleTargets()
{
	for (AActor* Target : this->TargetActors)
	{
		if (IsValid(Target) && Target->Implements<UActivatable>())
		{
			IActivatable::Execute_ToggleActivate(Target);
		}
	}
}

void AInteractSwitch::SetTargetIndicator(bool bShow)
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

// Called every frame
void AInteractSwitch::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AInteractSwitch::Interact_Implementation(AActor* InActor)
{
	if (!IsValid(InActor) || !InActor->Implements<UInteractor>())
	{
		return;
	}
	if (!this->HasActivatableTarget())
	{
		return;
	}
	// 한 번만 눌리는 발판이고 이미 눌렸을 때, 그리고 상호작용 쿨타임일 때
	if (bIsInteractCooldown || (!bIsReinteractable && bIsInteracted))
	{
		return;
	}
	IInteractor::Execute_PlayInteractMontage(InActor, this, nullptr);
	this->ActivateSwitch();
}

void AInteractSwitch::ActivateSwitch()
{
	// 시간 스위치를 켤 때만 긴 쿨타임을 걸고, 쿨타임이 끝나면 스스로 되돌린다. 되돌릴 때는 보통 쿨타임
	const bool	bIsTimedOn = (this->AutoReinteractTime > 0.0f) && !this->bIsAutoReturnPending;
	const float	CooldownTime = bIsTimedOn
		? FMath::Max(this->AutoReinteractTime, this->InteractInterval)
		: this->InteractInterval;

	this->bIsAutoReturnPending = bIsTimedOn;
	this->bIsInteractCooldown = true;
	this->bIsInteracted = true;
	GetWorldTimerManager().SetTimer(
		InteractionCooldownTimer,
		this,
		&AInteractSwitch::ResetInteractionCooldown,
		CooldownTime,
		false
	);
	this->RefreshInteractState();
	this->PlayInteractSound();
	this->PerformSwitchAction();
}

void AInteractSwitch::PerformSwitchAction()
{
	this->ToggleTargets();
	this->SaveSwitchStateIfSettled();
}

void AInteractSwitch::SaveSwitchStateIfSettled() const
{
	if (!this->bIsAutoReturnPending)
	{
		this->SaveSwitchState();
	}
}

void AInteractSwitch::PlayInteractSound() const
{
	if (this->InteractSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, this->InteractSound, GetActorLocation());
	}
}

FSavedActorState AInteractSwitch::MakeSaveState() const
{
	FSavedActorState State;
	State.bIsUsed = this->bIsInteracted;
	return State;
}

void AInteractSwitch::ApplySaveState(const FSavedActorState& State)
{
	this->bIsInteracted = State.bIsUsed;
	this->RefreshInteractState();
}

void AInteractSwitch::SaveSwitchState() const
{
	UGameInstance* GameInstance = GetGameInstance();
	UGameSaveSubsystem* Save = GameInstance
		? GameInstance->GetSubsystem<UGameSaveSubsystem>()
		: nullptr;
	if (Save)
	{
		Save->RecordActorState(this, this->MakeSaveState());
	}
}

void AInteractSwitch::ShowInteractionIndicator_Implementation()
{
	this->SelectCount++;
	this->RefreshHighlight();
}

void AInteractSwitch::HideInteractionIndicator_Implementation()
{
	this->SelectCount = FMath::Max(0, this->SelectCount - 1);
	this->RefreshHighlight();
}

bool AInteractSwitch::bIsInteractReady_Implementation()
{
	return (this->InteractState == EInteractState::Ready);
}

EInteractState AInteractSwitch::GetInteractState_Implementation()
{
	return (this->InteractState);
}

void AInteractSwitch::ResetInteractionCooldown()
{
	this->bIsInteractCooldown = false;

	// 시간 스위치: 켜 둔 시간이 끝났으니 스스로 한 번 더 작동해 대상을 되돌린다
	if (this->bIsAutoReturnPending)
	{
		this->ActivateSwitch();
		return;
	}
	this->RefreshInteractState();
}

void AInteractSwitch::RefreshInteractState()
{
	// 한 번만 누를 수 있는 스위치가 이미 눌렸으면 쿨다운 여부와 관계없이 Done
	if (!this->bIsReinteractable && this->bIsInteracted)
	{
		this->InteractState = EInteractState::Done;
	}
	else if (this->bIsInteractCooldown)
	{
		this->InteractState = EInteractState::Cooldown;
	}
	else
	{
		this->InteractState = EInteractState::Ready;
	}
	this->RefreshHighlight();
}

void AInteractSwitch::RefreshHighlight()
{
	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetHighlight(this->InteractState, (this->SelectCount > 0));
	}

	// 선택되어 있는 동안 이 스위치가 작동시킬 대상의 테두리도 켠다
	const bool	bShouldShowTarget = (this->SelectCount > 0) && this->InteractState != EInteractState::Done;
	if (bShouldShowTarget == this->bIsShowingTargetIndicator)
	{
		return ;
	}
	this->bIsShowingTargetIndicator = bShouldShowTarget;
	this->SetTargetIndicator(bShouldShowTarget);
}



