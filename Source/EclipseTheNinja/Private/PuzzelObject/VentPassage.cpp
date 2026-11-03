#include "PuzzelObject/VentPassage.h"
#include "Interface/Interactor.h"
#include "Component/InteractHighlightComponent.h"
#include "Character/PlayerCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

// Sets default values
AVentPassage::AVentPassage()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	this->RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(this->RootScene);

	this->MeshAssembly = CreateDefaultSubobject<UChildActorComponent>(TEXT("MeshAssembly"));
	this->MeshAssembly->SetupAttachment(GetRootComponent());

	this->EntryPoint = CreateDefaultSubobject<USceneComponent>(TEXT("EntryPoint"));
	this->EntryPoint->SetupAttachment(GetRootComponent());

	this->InteractHighlight = CreateDefaultSubobject<UInteractHighlightComponent>(TEXT("InteractHighlight"));

	this->InteractKeyWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractKeyWidget"));
	this->InteractKeyWidget->SetupAttachment(GetRootComponent());
	this->InteractKeyWidget->SetWidgetSpace(EWidgetSpace::Screen);
	this->InteractKeyWidget->SetDrawAtDesiredSize(true);
	this->InteractKeyWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	this->InteractKeyWidget->SetVisibility(false);
}

// Called when the game starts or when spawned
void AVentPassage::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetKeyWidget(this->InteractKeyWidget);
	}
	this->RefreshInteractState();
}

// Called every frame
void AVentPassage::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AVentPassage::Interact_Implementation(AActor* InActor)
{
	if (!IsValid(InActor) || !InActor->Implements<UInteractor>())
	{
		return;
	}
	if (!IsValid(this->TargetVent))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[AVentPassage::Interact_Implementation] 연결된 액터가 없습니다")
		);
		return;
	}
	if (this->bIsInteractCooldown)
	{
		return;
	}
	ACharacter* TargetCharacter = Cast<ACharacter>(InActor);
	if (!TargetCharacter)
	{
		return ;
	}
	// 지나가는 동안 반대쪽에서 다시 들어오지 못하도록 양쪽 모두 쿨다운을 건다
	this->StartInteractCooldown();
	this->TargetVent->StartInteractCooldown();

	LockTraveler(TargetCharacter);
	this->AlignToEntryPoint(TargetCharacter);

	UAnimInstance* AnimInstance = TargetCharacter->GetMesh() ? TargetCharacter->GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && this->EnterMontage && AnimInstance->Montage_Play(this->EnterMontage) > 0.f)
	{
		FOnMontageBlendingOutStarted	Delegate = FOnMontageBlendingOutStarted::CreateUObject(
			this,
			&AVentPassage::OnEnterMontageBlendingOut,
			TWeakObjectPtr<ACharacter>(TargetCharacter)
		);
		AnimInstance->Montage_SetBlendingOutDelegate(Delegate, this->EnterMontage);
		return ;
	}
	// 몽타주가 없거나 재생에 실패하면 바로 옮긴다
	this->TargetVent->OutVent(TargetCharacter);
}

void AVentPassage::ShowInteractionIndicator_Implementation()
{
	this->SelectCount++;
	this->RefreshHighlight();
}

void AVentPassage::HideInteractionIndicator_Implementation()
{
	this->SelectCount = FMath::Max(0, this->SelectCount - 1);
	this->RefreshHighlight();
}

bool AVentPassage::bIsInteractReady_Implementation()
{
	return (this->InteractState == EInteractState::Ready);
}

EInteractState AVentPassage::GetInteractState_Implementation()
{
	return (this->InteractState);
}

void AVentPassage::ResetInteractionCooldown()
{
	this->bIsInteractCooldown = false;
	this->RefreshInteractState();
}

void AVentPassage::RefreshInteractState()
{
	this->InteractState = this->bIsInteractCooldown ? EInteractState::Cooldown : EInteractState::Ready;
	this->RefreshHighlight();
}

void AVentPassage::RefreshHighlight()
{
	const bool	bIsSelected = (this->SelectCount > 0);
	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetHighlight(this->InteractState, bIsSelected || this->bIsLinkedSelected);
	}
	// 반대쪽 때문에 강조된 것이면 E키로 상호작용할 대상이 아니므로 키 위젯은 숨긴다
	if (!bIsSelected && IsValid(this->InteractKeyWidget))
	{
		this->InteractKeyWidget->SetVisibility(false);
	}

	// 직접 선택되었을 때만 반대쪽 벤트도 함께 강조한다
	if (bIsSelected == this->bIsShowingLinkedTarget)
	{
		return ;
	}
	this->bIsShowingLinkedTarget = bIsSelected;
	if (IsValid(this->TargetVent))
	{
		this->TargetVent->SetLinkedSelected(bIsSelected);
	}
}

void AVentPassage::SetLinkedSelected(bool InbIsLinkedSelected)
{
	this->bIsLinkedSelected = InbIsLinkedSelected;
	this->RefreshHighlight();
}

void AVentPassage::StartInteractCooldown()
{
	this->bIsInteractCooldown = true;
	GetWorldTimerManager().SetTimer(
		InteractionCooldownTimer,
		this,
		&AVentPassage::ResetInteractionCooldown,
		this->InteractInterval,
		false
	);
	this->RefreshInteractState();
}

void AVentPassage::OutVent(ACharacter* InCharacter)
{
	if (!IsValid(InCharacter))
	{
		return ;
	}
	this->StartInteractCooldown();
	this->AlignToEntryPoint(InCharacter);

	UAnimInstance* AnimInstance = InCharacter->GetMesh() ? InCharacter->GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && this->EnterMontage
		&& AnimInstance->Montage_Play(this->EnterMontage, -1.f, EMontagePlayReturnType::MontageLength, this->EnterMontage->GetPlayLength()) > 0.f)
	{
		FOnMontageEnded	Delegate = FOnMontageEnded::CreateUObject(
			this,
			&AVentPassage::OnExitMontageEnded,
			TWeakObjectPtr<ACharacter>(InCharacter)
		);
		AnimInstance->Montage_SetEndDelegate(Delegate, this->EnterMontage);
		return ;
	}
	// 몽타주가 없거나 재생에 실패하면 바로 풀어 준다
	UnlockTraveler(InCharacter);
}

void AVentPassage::AlignToEntryPoint(ACharacter* InCharacter) const
{
	// EntryPoint는 발 위치이므로 캡슐 중심 높이만큼 올린다
	const float		HalfHeight = InCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector	Location = this->EntryPoint->GetComponentLocation() + FVector(0.f, 0.f, HalfHeight);
	const float		Yaw = this->EntryPoint->GetComponentRotation().Yaw;

	InCharacter->SetActorLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);

	UCharacterMovementComponent*	Movement = InCharacter->GetCharacterMovement();
	if (Movement && Movement->bConstrainToPlane)
	{
		// 반대쪽 벤트가 다른 깊이에 있을 수 있으므로 평면 고정 기준을 옮긴다
		Movement->SetPlaneConstraintOrigin(Location);
	}
	// 플레이어는 매 틱 바라볼 방향으로 몸을 돌리므로 그 방향도 같이 맞춘다
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(InCharacter))
	{
		Player->SetAssassinationFacing(Yaw);
	}
}

void AVentPassage::OnEnterMontageBlendingOut(UAnimMontage* InMontage, bool bInterrupted, TWeakObjectPtr<ACharacter> InCharacter)
{
	ACharacter*	Character = InCharacter.Get();
	if (!Character)
	{
		return ;
	}
	if (bInterrupted || !IsValid(this->TargetVent))
	{
		UnlockTraveler(Character);
		return ;
	}
	this->TargetVent->OutVent(Character);
}

void AVentPassage::OnExitMontageEnded(UAnimMontage* InMontage, bool bInterrupted, TWeakObjectPtr<ACharacter> InCharacter)
{
	if (ACharacter* Character = InCharacter.Get())
	{
		UnlockTraveler(Character);
	}
}

void AVentPassage::LockTraveler(ACharacter* InCharacter)
{
	UCharacterMovementComponent*	Movement = InCharacter->GetCharacterMovement();
	Movement->StopMovementImmediately();
	// 이동 중 중력과 이동 입력의 영향을 받지 않게 한다
	Movement->SetMovementMode(MOVE_None);
	if (APlayerController* PlayerController = Cast<APlayerController>(InCharacter->GetController()))
	{
		InCharacter->DisableInput(PlayerController);
	}
}

void AVentPassage::UnlockTraveler(ACharacter* InCharacter)
{
	// 땅이 없으면 CMC가 다음 틱에 Falling으로 바꾼다
	InCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (APlayerController* PlayerController = Cast<APlayerController>(InCharacter->GetController()))
	{
		InCharacter->EnableInput(PlayerController);
	}
}
