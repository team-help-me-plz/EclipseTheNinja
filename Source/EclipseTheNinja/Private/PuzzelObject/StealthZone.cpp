#include "PuzzelObject/StealthZone.h"
#include "Character/PlayerCharacter.h"
#include "Component/HidingComponent.h"
#include "Component/InteractHighlightComponent.h"
#include "Subsystem/UIMessageSubsystem.h"

#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"

AStealthZone::AStealthZone()
{
	PrimaryActorTick.bCanEverTick = false;

	this->RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(this->RootScene);

	this->InteractArea = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractArea"));
	this->InteractArea->SetupAttachment(GetRootComponent());
	this->InteractArea->SetBoxExtent(FVector(60.f, 100.f, 100.f));
	this->InteractArea->SetRelativeLocation(FVector(0.f, -75.f, 0.f));
	this->InteractArea->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	this->InteractArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	this->InteractArea->SetCollisionObjectType(ECC_WorldDynamic);
	this->InteractArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	this->InteractArea->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	this->InteractArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// 기본 은신 깊이는 150이다
	this->HidePoint = CreateDefaultSubobject<USceneComponent>(TEXT("HidePoint"));
	this->HidePoint->SetupAttachment(GetRootComponent());
	this->HidePoint->SetRelativeLocation(FVector(0.f, -150.f, 0.f));

	this->InteractHighlight = CreateDefaultSubobject<UInteractHighlightComponent>(TEXT("InteractHighlight"));

	this->InteractKeyWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractKeyWidget"));
	this->InteractKeyWidget->SetupAttachment(GetRootComponent());
	this->InteractKeyWidget->SetWidgetSpace(EWidgetSpace::Screen);
	this->InteractKeyWidget->SetDrawAtDesiredSize(true);
	this->InteractKeyWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	this->InteractKeyWidget->SetVisibility(false);
}

void AStealthZone::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetKeyWidget(this->InteractKeyWidget);
	}
	this->RefreshHighlight();
}

void AStealthZone::Interact_Implementation(AActor* InActor)
{
	// 분신은 숨지 않는다
	APlayerCharacter* Player = Cast<APlayerCharacter>(InActor);
	if (!IsValid(Player))
	{
		return;
	}
	if (Player->GetPlayerCondition() == EPlayerCondition::Combat)
	{
		UUIMessageSubsystem::ShowMessage(Cast<APlayerController>(Player->GetController()), this->CannotHideText);
		return;
	}
	UHidingComponent* Hiding = Player->FindComponentByClass<UHidingComponent>();
	if (IsValid(Hiding))
	{
		Hiding->TryEnter(this);
	}
}

void AStealthZone::ShowInteractionIndicator_Implementation()
{
	this->SelectCount++;
	this->RefreshHighlight();
}

void AStealthZone::HideInteractionIndicator_Implementation()
{
	this->SelectCount = FMath::Max(0, this->SelectCount - 1);
	this->RefreshHighlight();
}

bool AStealthZone::bIsInteractReady_Implementation()
{
	// 상호작용 박스는 겹치기 시작할 때만 이 값을 확인한다. 전투가 끝난 뒤에도 선택될 수 있도록 항상 true로 두고,
	// 전투 중에는 Interact에서 숨기를 거절하고 메시지를 띄운다.
	return (true);
}

EInteractState AStealthZone::GetInteractState_Implementation()
{
	// 전투 중에도 E를 눌러 숨을 수 없다는 메시지를 볼 수 있도록 키 위젯을 계속 보여 준다
	return (EInteractState::Ready);
}

FVector AStealthZone::GetHideLocation() const
{
	return (this->HidePoint->GetComponentLocation());
}

void AStealthZone::RefreshHighlight()
{
	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetHighlight(this->GetInteractState_Implementation(), (this->SelectCount > 0));
	}
}
