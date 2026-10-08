#include "ProjComponent/AbilityTask_WaitCloneTargeting.h"
#include "Interface/Interactable.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UAbilityTask_WaitCloneTargeting::UAbilityTask_WaitCloneTargeting()
{
	this->bTickingTask = true;
}

UAbilityTask_WaitCloneTargeting* UAbilityTask_WaitCloneTargeting::WaitCloneTarget(
	UGameplayAbility* OwningAbility, UInputMappingContext* MappingContext,
	UInputAction* AcceptAction, UInputAction* CancelAction,
	int32	MappingPriority, float SlowMotionScale)
{
	if (!IsValid(OwningAbility))
	{
		return (nullptr);
	}

	UAbilityTask_WaitCloneTargeting*	Task = NewAbilityTask<UAbilityTask_WaitCloneTargeting>(OwningAbility);
	Task->TargetingMappingContext = MappingContext;
	Task->AcceptInputAction = AcceptAction;
	Task->CancelInputAction = CancelAction;
	Task->TargetingMappingPriority = MappingPriority;
	Task->TargetingTimeDilation = FMath::Clamp(SlowMotionScale, 0.01f, 1.0f);
	return (Task);
}

void UAbilityTask_WaitCloneTargeting::Activate()
{
	Super::Activate();

	if (!this->Ability)
	{
		HandleCancel();
		return ;
	}
	if (!IsValid(this->TargetingMappingContext.Get()) || !IsValid(this->AcceptInputAction.Get()) || !IsValid(this->CancelInputAction.Get()))
	{
		HandleCancel();
		return ;
	}
	if (this->AcceptInputAction == this->CancelInputAction)
	{
		HandleCancel();
		return ;
	}

	const FGameplayAbilityActorInfo* ActorInfo = this->Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		HandleCancel();
		return ;
	}
	APlayerController*	PC = ActorInfo->PlayerController.Get();
	APawn*	AvatarPawn = Cast<APawn>(this->Ability->GetAvatarActorFromActorInfo());
	if (!IsValid(PC) || !PC->IsLocalController())
	{
		HandleCancel();
		return ;
	}
	if (!IsValid(AvatarPawn))
	{
		HandleCancel();
		return ;
	}

	UEnhancedInputComponent*	InputComponent = Cast<UEnhancedInputComponent>(AvatarPawn->InputComponent);
	ULocalPlayer*				LocalPlayer = PC->GetLocalPlayer();
	if (!LocalPlayer)
	{
		HandleCancel();
		return ;
	}
	if (!InputComponent)
	{
		HandleCancel();
		return ;
	}
	UEnhancedInputLocalPlayerSubsystem*	Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem || Subsystem->HasMappingContext(this->TargetingMappingContext.Get()))
	{
		HandleCancel();
		return ;
	}

	this->PlayerController = PC;
	this->EnhancedInputComponent = InputComponent;
	this->InputSubsystem = Subsystem;

	this->InputBindingHandles.Add(InputComponent->BindAction(
		this->AcceptInputAction.Get(), ETriggerEvent::Started,
		this, &UAbilityTask_WaitCloneTargeting::HandleAccept).GetHandle());
	this->InputBindingHandles.Add(InputComponent->BindAction(
		this->CancelInputAction.Get(), ETriggerEvent::Started,
		this, &UAbilityTask_WaitCloneTargeting::HandleCancel).GetHandle());

	FModifyContextOptions	Options;
	Options.bIgnoreAllPressedKeysUntilRelease = true;
	Subsystem->AddMappingContext(this->TargetingMappingContext.Get(), this->TargetingMappingPriority, Options);
	this->bMappingContextAdded = true;

	this->PreviousTimeDilation = UGameplayStatics::GetGlobalTimeDilation(this);
	this->bTimeDilationChanged = true;
	UGameplayStatics::SetGlobalTimeDilation(this, this->PreviousTimeDilation * this->TargetingTimeDilation);
	UpdateTargetCandidate();
}

void UAbilityTask_WaitCloneTargeting::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	UpdateTargetCandidate();
}

void UAbilityTask_WaitCloneTargeting::UpdateTargetCandidate()
{
	this->bHasValidTarget = false;
	this->PendingCommand = FCloneCommandType{};
	APlayerController*	PC = this->PlayerController.Get();
	UWorld*				World = GetWorld();
	AActor*				Avatar = this->Ability ? this->Ability->GetAvatarActorFromActorInfo() : nullptr;
	if (!PC || !World || !IsValid(Avatar))
	{
		return ;
	}

	FVector	RayOrigin;
	FVector	RayDirection;
	if (!PC->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return ;
	}

	FCollisionQueryParams	QueryParams(SCENE_QUERY_STAT(CloneTargeting), false);

	QueryParams.AddIgnoredActor(Avatar);
	FHitResult	CursorHit;
	const bool	bCursorHit = World->LineTraceSingleByChannel(
		CursorHit, RayOrigin, RayOrigin + RayDirection * 100000.0f,
		ECC_Visibility, QueryParams);

	// 마우스 광선과 사이드뷰 이동 평면(Y 고정)의 교점 계산
	if (FMath::IsNearlyZero(RayDirection.Y))
	{
		return ;
	}
	const double	PlaneY = Avatar->GetActorLocation().Y;
	const double	DistanceToPlane = (PlaneY - RayOrigin.Y) / RayDirection.Y;
	if (DistanceToPlane < 0.0)
	{
		return ;
	}
	FVector	CursorOnPlane = RayOrigin + RayDirection * DistanceToPlane;
	CursorOnPlane.Y = PlaneY;

	FHitResult	GroundHit;
	const bool	bGroundHit = World->LineTraceSingleByChannel(
		GroundHit, CursorOnPlane, CursorOnPlane - FVector(0.0, 0.0, 10000.0),
		ECC_Visibility, QueryParams);
	if (!bGroundHit)
	{
		return ;
	}
	if (GroundHit.bStartPenetrating)
	{
		return ;
	}
	AActor* HitActor = CursorHit.GetActor();
	FVector	TargetLocation = GroundHit.ImpactPoint + FVector(0.f, 0.f, 90.f);
	if (IsValid(HitActor) && HitActor->Implements<UInteractable>())
	{
		this->PendingCommand = FCloneCommandType::MakeInteract(HitActor, TargetLocation);
		this->bHasValidTarget = true;
		return;
	}
	this->PendingCommand = FCloneCommandType::MakeMove(TargetLocation);
	this->bHasValidTarget = true;
}

void UAbilityTask_WaitCloneTargeting::HandleAccept()
{
	if (this->bSelectionFinished)
	{
		return ;
	}
	UpdateTargetCandidate();
	if (!this->bHasValidTarget)
	{
		return ;
	}
	const FCloneCommandType	SelectedCommand = this->PendingCommand;
	this->bSelectionFinished = true;
	CleanupTargeting();
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		this->OnAccepted.Broadcast(SelectedCommand);
	}
	if (!IsFinished())
	{
		EndTask();
	}
}

void UAbilityTask_WaitCloneTargeting::HandleCancel()
{
	if (this->bSelectionFinished)
	{
		return ;
	}
	this->bSelectionFinished = true;
	CleanupTargeting();
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		this->OnCancelled.Broadcast();
	}
	if (!IsFinished())
	{
		EndTask();
	}
}

void UAbilityTask_WaitCloneTargeting::CleanupTargeting()
{
	if (UEnhancedInputComponent* InputComponent = this->EnhancedInputComponent.Get())
	{
		for (const uint32 Handle : this->InputBindingHandles)
		{
			InputComponent->RemoveBindingByHandle(Handle);
		}
	}
	this->InputBindingHandles.Reset();
	if (this->bMappingContextAdded)
	{
		this->bMappingContextAdded = false;
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = this->InputSubsystem.Get())
		{
			FModifyContextOptions	Options;
			Options.bIgnoreAllPressedKeysUntilRelease = true;
			Subsystem->RemoveMappingContext(this->TargetingMappingContext.Get(), Options);
		}
	}
	if (this->bTimeDilationChanged)
	{
		this->bTimeDilationChanged = false;
		if (GetWorld())
		{
			UGameplayStatics::SetGlobalTimeDilation(this, this->PreviousTimeDilation);
		}
	}
	this->bHasValidTarget = false;
	this->PendingCommand = FCloneCommandType{};
}

void UAbilityTask_WaitCloneTargeting::OnDestroy(bool bInOwnerFinished)
{
	this->bSelectionFinished = true;
	CleanupTargeting();
	Super::OnDestroy(bInOwnerFinished);
}