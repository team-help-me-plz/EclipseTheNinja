


#include "PuzzelObject/LeverSwitch.h"
#include "Interface/Interactor.h"
#include "Interface/Activatable.h"
#include "Subsystem/UIMessageSubsystem.h"

#include "Components/BoxComponent.h"

ALeverSwitch::ALeverSwitch()
{
	this->LeverMesh = CreateDefaultSubobject<USkeletalMeshComponent>("LeverMesh");
	this->LeverMesh->SetupAttachment(GetRootComponent());
	this->LeverMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	// 외형만 담당한다. 판정은 InteractCollision이 맡는다
	this->LeverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	this->InteractCollision = CreateDefaultSubobject<UBoxComponent>("InteractCollision");
	this->InteractCollision->SetupAttachment(GetRootComponent());
	this->InteractCollision->SetBoxExtent(FVector(40.f, 25.f, 65.f));
	this->InteractCollision->SetRelativeLocation(FVector(0.f, 0.f, 65.f));
	this->InteractCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	this->InteractCollision->SetCollisionObjectType(ECC_WorldDynamic);
	this->InteractCollision->SetCollisionResponseToAllChannels(ECR_Overlap);
	// 분신 조준 커서 트레이스(Visibility)에 걸리도록 부모 BeginPlay의 스태틱 메시 설정과 맞춘다
	this->InteractCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	this->InteractCollision->SetGenerateOverlapEvents(true);
}

void ALeverSwitch::Interact_Implementation(AActor* InActor)
{
	if (!IsValid(InActor) || !InActor->Implements<UInteractor>())
	{
		return;
	}
	if (!this->TargetActor || !this->TargetActor->Implements<UActivatable>())
	{
		return;
	}
	if (this->LockedVineCount != 0)
	{
		APlayerController*	PC = Cast<APlayerController>(InActor->GetInstigatorController());
		if (PC)
		{
			UUIMessageSubsystem::ShowMessage(PC, FText::FromString(TEXT("덩굴에 감겨있습니다.")));
		}
		return ;
	}
	// 레버가 넘어가는 중이거나, 한 번만 당길 수 있는 레버를 이미 당겼을 때
	if (this->bIsInteractCooldown || (!this->bIsReinteractable && this->bIsInteracted))
	{
		return ;
	}
	this->bIsInteractCooldown = true;
	this->bIsInteracted = true;
	this->bIsToggled = !this->bIsToggled;
	this->PlayLeverAnim();
	GetWorldTimerManager().SetTimer(
		InteractionCooldownTimer,
		this,
		&ALeverSwitch::ResetInteractionCooldown,
		this->InteractInterval,
		false
	);
	this->RefreshInteractState();
}

void ALeverSwitch::ResetToInitial_Implementation()
{
	Super::ResetToInitial_Implementation();

	// Off 자세로 되돌린다
	this->bIsToggled = false;
	if (this->LeverSequence)
	{
		this->LeverMesh->SetPosition(0.f, false);
		this->LeverMesh->Stop();
	}
}

void ALeverSwitch::AddInteractLock_Implementation()
{
	this->LockedVineCount++;
}

void ALeverSwitch::RemoveInteractLock_Implementation()
{
	this->LockedVineCount--;
}

bool ALeverSwitch::IsInteractLocked_Implementation()
{
	return (this->LockedVineCount > 0);
}

void ALeverSwitch::BeginPlay()
{
	Super::BeginPlay();

	if (this->LeverSequence)
	{
		this->SequenceLength = this->LeverSequence->GetPlayLength();
		this->PlayRate = this->SequenceLength / this->InteractInterval;

		// 시퀀스를 연결해 두어야 Play가 동작한다. Off 자세(0초)에서 멈춰 둔다
		this->LeverMesh->SetAnimation(this->LeverSequence);
		this->LeverMesh->SetPosition(0.f, false);
		this->LeverMesh->Stop();
	}
}

void ALeverSwitch::ResetInteractionCooldown()
{
	// 레버가 다 넘어간 뒤에 대상을 작동시킨다
	Super::ResetInteractionCooldown();
	IActivatable::Execute_ToggleActivate(this->TargetActor);
}

void ALeverSwitch::PlayLeverAnim()
{
	if (this->LeverSequence && this->LeverMesh)
	{
		this->LeverMesh->SetPlayRate(this->bIsToggled ? this->PlayRate : -this->PlayRate);
		this->LeverMesh->SetPosition(this->bIsToggled ? 0.f : this->SequenceLength, false);
		this->LeverMesh->Play(false);
	}
}
