#include "Component/HidingComponent.h"
#include "Character/PlayerCharacter.h"
#include "Character/PlayerCharacterController.h"
#include "PuzzelObject/StealthZone.h"
#include "Subsystem/UIMessageSubsystem.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "TimerManager.h"

UHidingComponent::UHidingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHidingComponent::BeginPlay()
{
	Super::BeginPlay();

	this->Player = Cast<APlayerCharacter>(GetOwner());
	if (APlayerCharacter* OwnerPlayer = this->Player.Get())
	{
		OwnerPlayer->OnPlayerConditionChanged.AddUObject(this, &UHidingComponent::OnPlayerConditionChanged);
	}
}

bool UHidingComponent::TryEnter(const AStealthZone* InZone)
{
	APlayerCharacter* OwnerPlayer = this->Player.Get();
	if (this->Phase != EHidingPhase::None || !IsValid(InZone) || !IsValid(OwnerPlayer)) return false;

	UCharacterMovementComponent* Movement = OwnerPlayer->GetCharacterMovement();
	if (!Movement->IsMovingOnGround()) return false;
	if (!OwnerPlayer->TryEnterHiding()) return false;

	this->Phase = EHidingPhase::Entering;
	this->bExitAfterEnter = false;
	this->ActiveZone = InZone;
	SetInputLocked(true);

	// 높이는 그대로 두고 은신 지점의 X, Y로만 들어간다
	this->HideLocation = InZone->GetHideLocation();
	this->HideLocation.Z = OwnerPlayer->GetActorLocation().Z;
	// 이동 평면 원점은 순간이동(리스폰, 분신 교환 등) 뒤에 갱신되지 않으므로 실제 서 있던 Y로 돌아온다
	this->OutsideY = OwnerPlayer->GetActorLocation().Y;

	// 안쪽(-Y)으로 들어가야 하므로 이동하는 동안 평면 고정을 푼다
	Movement->StopMovementImmediately();
	this->bSavedPlaneConstraint = Movement->bConstrainToPlane;
	Movement->SetPlaneConstraintEnabled(false);

	// 들어가고, 숨어 있고, 나오는 동안(EndHiding까지) 장애물 메시와 다른 캐릭터를 통과한다.
	// 바닥(WorldStatic)은 그대로 밟는다
	UCapsuleComponent* Capsule = OwnerPlayer->GetCapsuleComponent();
	this->SavedPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	this->SavedObstacleResponse = Capsule->GetCollisionResponseToChannel(this->ObstacleChannel);
	Capsule->SetCollisionResponseToChannel(this->ObstacleChannel, ECR_Ignore);

	// +X를 바라보면 왼쪽이 -Y다
	UAnimMontage* Montage = OwnerPlayer->GetActorForwardVector().X >= 0.f ? this->LeftMontage : this->RightMontage;
	UAnimInstance* AnimInstance = OwnerPlayer->GetMesh() ? OwnerPlayer->GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && Montage && AnimInstance->Montage_Play(Montage) > 0.f)
	{
		this->ActiveMontage = Montage;
		// 위치는 코드가 맡는다. 루트 모션을 추출만 하고 적용하지 않아야 메시 루트가 캡슐에 붙어 있는다
		this->SavedRootMotionMode = AnimInstance->RootMotionMode;
		this->bRootMotionModeOverridden = true;
		AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &UHidingComponent::OnMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
	}

	const float Duration = GetSectionTime(this->StartSection);
	MoveTo(this->HideLocation, Duration);
	GetWorld()->GetTimerManager().SetTimer(this->PhaseTimer, this, &UHidingComponent::OnEnterFinished, Duration, false);
	return true;
}

void UHidingComponent::RequestExit()
{
	if (this->Phase != EHidingPhase::Hidden) return;

	// 다른 캐릭터가 앞을 막고 있으면 나가지 않는다
	const AStealthZone* Zone = this->ActiveZone.Get();
	APlayerCharacter* OwnerPlayer = this->Player.Get();
	if (Zone && Zone->IsExitBlocked(OwnerPlayer))
	{
		UUIMessageSubsystem::ShowMessage(Cast<APlayerController>(OwnerPlayer->GetController()), Zone->GetCannotExitText());
		return;
	}
	BeginExit();
}

void UHidingComponent::BeginExit()
{
	APlayerCharacter* OwnerPlayer = this->Player.Get();
	if (!IsValid(OwnerPlayer)) return;

	this->Phase = EHidingPhase::Exiting;
	// 나오기 시작하면 다시 감지될 수 있다
	OwnerPlayer->ExitHiding();

	UAnimInstance* AnimInstance = OwnerPlayer->GetMesh() ? OwnerPlayer->GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && this->ActiveMontage)
	{
		AnimInstance->Montage_JumpToSection(this->EndSection, this->ActiveMontage);
	}

	// 들어가기 전에 서 있던 Y로 돌아간다
	FVector OutsideLocation = this->HideLocation;
	OutsideLocation.Y = this->OutsideY;

	const float Duration = GetSectionTime(this->EndSection);
	MoveTo(OutsideLocation, Duration);
	GetWorld()->GetTimerManager().SetTimer(this->PhaseTimer, this, &UHidingComponent::OnExitFinished, Duration, false);
}

void UHidingComponent::ForceExit()
{
	if (this->Phase == EHidingPhase::None) return;
	EndHiding(true);
}

void UHidingComponent::OnEnterFinished()
{
	if (this->Phase != EHidingPhase::Entering) return;

	this->Phase = EHidingPhase::Hidden;
	if (this->bExitAfterEnter)
	{
		BeginExit();
	}
}

void UHidingComponent::OnExitFinished()
{
	// End 섹션은 끝까지 재생되도록 두고 이동과 상태만 정리한다
	EndHiding(false);
}

void UHidingComponent::EndHiding(bool bStopMontage)
{
	// 몽타주 종료 알림이 다시 들어오지 않도록 단계부터 되돌린다
	this->Phase = EHidingPhase::None;
	this->bExitAfterEnter = false;
	GetWorld()->GetTimerManager().ClearTimer(this->PhaseTimer);

	APlayerCharacter* OwnerPlayer = this->Player.Get();
	if (!IsValid(OwnerPlayer)) return;

	UCharacterMovementComponent* Movement = OwnerPlayer->GetCharacterMovement();
	if (this->MoveSourceID != 0)
	{
		Movement->RemoveRootMotionSourceByID(this->MoveSourceID);
		this->MoveSourceID = 0;
	}

	UAnimInstance* AnimInstance = OwnerPlayer->GetMesh() ? OwnerPlayer->GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && this->ActiveMontage)
	{
		// 다음에 같은 몽타주로 다시 숨을 때 이전 재생의 종료 알림이 끼어들지 않게 한다
		if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(this->ActiveMontage))
		{
			MontageInstance->OnMontageEnded.Unbind();
		}
		if (bStopMontage)
		{
			AnimInstance->Montage_Stop(0.1f, this->ActiveMontage);
		}
	}
	this->ActiveMontage = nullptr;
	if (AnimInstance && this->bRootMotionModeOverridden)
	{
		AnimInstance->SetRootMotionMode(this->SavedRootMotionMode);
	}
	this->bRootMotionModeOverridden = false;

	// 들어가기 전 Y로 정확히 맞춘 뒤 평면 고정을 되돌린다
	FVector Location = OwnerPlayer->GetActorLocation();
	Location.Y = this->OutsideY;
	OwnerPlayer->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	Movement->StopMovementImmediately();
	Movement->SetPlaneConstraintEnabled(this->bSavedPlaneConstraint);
	// 원래 자리로 돌아온 뒤에 장애물, 캐릭터 충돌을 되돌린다
	UCapsuleComponent* Capsule = OwnerPlayer->GetCapsuleComponent();
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, this->SavedPawnResponse);
	Capsule->SetCollisionResponseToChannel(this->ObstacleChannel, this->SavedObstacleResponse);
	this->ActiveZone.Reset();

	SetInputLocked(false);
	OwnerPlayer->ExitHiding();
}

void UHidingComponent::SetInputLocked(bool bLocked)
{
	APlayerCharacter* OwnerPlayer = this->Player.Get();
	APlayerCharacterController* APC = OwnerPlayer ? Cast<APlayerCharacterController>(OwnerPlayer->GetController()) : nullptr;
	if (!APC) return;

	static const FName HidingLock(TEXT("Hiding"));
	if (!bLocked)
	{
		APC->PopInputLock(HidingLock);
	}
	else if (this->HidingMappingContext)
	{
		APC->PushInputLock(HidingLock, this->HidingMappingContext);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UHidingComponent::SetInputLocked] HidingMappingContext가 비어 있어 은신 중 입력을 잠그지 않습니다"));
	}
}

void UHidingComponent::MoveTo(const FVector& InTarget, float InDuration)
{
	APlayerCharacter* OwnerPlayer = this->Player.Get();
	if (!IsValid(OwnerPlayer)) return;

	UCharacterMovementComponent* Movement = OwnerPlayer->GetCharacterMovement();
	if (this->MoveSourceID != 0)
	{
		Movement->RemoveRootMotionSourceByID(this->MoveSourceID);
	}

	TSharedPtr<FRootMotionSource_MoveToForce> MoveToForce = MakeShared<FRootMotionSource_MoveToForce>();
	MoveToForce->InstanceName = TEXT("HidingMove");
	MoveToForce->AccumulateMode = ERootMotionAccumulateMode::Override;
	MoveToForce->Priority = 1000;
	MoveToForce->StartLocation = OwnerPlayer->GetActorLocation();
	MoveToForce->TargetLocation = InTarget;
	MoveToForce->Duration = InDuration;
	MoveToForce->bRestrictSpeedToExpected = false;
	MoveToForce->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	MoveToForce->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	this->MoveSourceID = Movement->ApplyRootMotionSource(MoveToForce);
}

float UHidingComponent::GetSectionTime(FName InSection) const
{
	if (this->ActiveMontage)
	{
		const int32 SectionIndex = this->ActiveMontage->GetSectionIndex(InSection);
		if (SectionIndex != INDEX_NONE)
		{
			return (this->ActiveMontage->GetSectionLength(SectionIndex));
		}
	}
	return (this->FallbackMoveTime);
}

void UHidingComponent::OnMontageEnded(UAnimMontage* InMontage, bool bInterrupted)
{
	// 숨어 있는 동안 피격 등으로 몽타주가 끊기면 그 자리에 굳지 않도록 바로 나온다
	if (InMontage == this->ActiveMontage
		&& (this->Phase == EHidingPhase::Entering || this->Phase == EHidingPhase::Hidden))
	{
		ForceExit();
	}
}

void UHidingComponent::OnPlayerConditionChanged(EPlayerCondition InCondition)
{
	// 발각되어 전투가 되면 끊지 않고 연출대로 걸어 나온다
	if (InCondition != EPlayerCondition::Combat) return;

	switch (this->Phase)
	{
	case EHidingPhase::Entering:
		this->bExitAfterEnter = true;
		break;
	case EHidingPhase::Hidden:
		BeginExit();
		break;
	default:
		// 나오는 중이면 그대로 끝까지 나온다
		break;
	}
}
